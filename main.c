/*******************************************************************************
 * BSD 3-Clause License
 *
 * Copyright (c) 2017, Matt Davis (enferex) https://github.com/enferex
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * * Neither the name of the copyright holder nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 ******************************************************************************/

#define __USE_MISC
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <endian.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>


/* Macros to handle endiness of tiff. 4 cases:
 * 1) BE system and TIFF is BE
 * 2) BE system and TIFF is LE
 * 3) LE system and TIFF is BE
 * 4) LE system and TIFF is LE
 */
#define ORDER_BE(_h) (_h->hdr.byte_order == 0x4D4D)
#if __BYTE_ORDER == __BIG_ENDIAN
#define NATIVE2(_h,_v) (ORDER_BE(_h) ? (_v) : le16toh(_v))
#define NATIVE4(_h,_v) (ORDER_BE(_h) ? (_v) : le32toh(_v))
#elif __BYTE_ORDER == __LITTLE_ENDIAN
#define NATIVE2(_h,_v) (ORDER_BE(_h) ? htobe16(_v) : (_v))
#define NATIVE4(_h,_v) (ORDER_BE(_h) ? htobe32(_v) : (_v))
#else
#error "Middle endian not supported."
#endif


/* Error handling */
#define ERR(_expr, _cmpr, ...)                                          \
    do {                                                                \
      if ((_expr) _cmpr) {                                              \
          fprintf(stderr, "Error: " __VA_ARGS__);                       \
          if (errno)                                                    \
            fprintf(stderr, "Error(%d): %s\n", errno, strerror(errno)); \
          else                                                          \
            fputc('\n', stderr);                                        \
          exit(EXIT_FAILURE);                                           \
      }                                                                 \
    } while (0)


/* Directoru Entry */
typedef struct _dir_ent_t
{
    uint16_t tag;
    uint16_t type;
    uint32_t count;
    uint32_t offset;
} dir_ent_t;

    
/* Image File Directory */
typedef struct _ifd_hdr_t
{
    int16_t count; /* Number of directory entries */
} ifd_hdr_t;


/* Image File Directory */
typedef struct _ifd_t
{
    ifd_hdr_t hdr;
    dir_ent_t *entries;
    uint32_t file_offset; /* Byte offset in file to this ifd */
    uint32_t next_ifd;    /* Byte offset to next ifd i       */
    struct _ifd_t *next;
} ifd_t;


/* TIFF Header */
typedef struct _tiff_hdr_t
{
    uint16_t byte_order;
    uint16_t universe;  /* Must be 42 */
    uint32_t first_ifd; /* Offset of first ifd */
} tiff_hdr_t;


/* TIFF */
typedef struct _tiff_t
{
    tiff_hdr_t hdr;
    const char *fname;
    int fd;
    ifd_t *ifds;
    struct _tiff_t *next;
} tiff_t;


/* Given a TIFF, locate all security flags */
static void locate_security_bits(const tiff_t *tiff)
{
   uint32_t i, sec_flag_count;
   const ifd_t *ifd;
   const dir_ent_t *ent;

   /* For each IFD */
   sec_flag_count = 0;
   for (ifd=tiff->ifds; ifd; ifd=ifd->next) {
       /* For each directory entry */
       for (i=0; i<ifd->hdr.count; ++i) {
           ent = (dir_ent_t *)&ifd->entries[i];
           if (NATIVE2(tiff, ent->tag) == 0x9212) {
               ++sec_flag_count;
               printf("%s: SecurityClassification tag: 0x%02x)\n",
                      tiff->fname, ent->type);
           }
       }
   }

   if (sec_flag_count == 0)
     printf("%s: SecurityClassification NOT found\n", tiff->fname);
}


/* Read an Image File Directory from disk */
static ifd_t *read_ifd(const tiff_t *tiff, FILE *fp, uint32_t offset)
{
    ifd_t *ifd;
    dir_ent_t *ents;
    uint32_t n_entries;

    /* IFD header */
    ERR(ifd = calloc(1, sizeof(ifd_t)), == NULL,
        "Not enough memory to store IFD");
    ERR(fseek(fp, offset, SEEK_SET), == -1,
        "Could not seek to initial IFD");
    ERR(fread(&ifd->hdr, 1, sizeof(ifd_hdr_t), fp), != sizeof(ifd_hdr_t),
        "Could not read IFD header");
    ifd->file_offset = offset;

    /* Directory entries */
    n_entries = NATIVE2(tiff, ifd->hdr.count);
    ERR(ents = calloc(n_entries, sizeof(dir_ent_t)), == NULL ,
        "Not enough memory to store IFD entries");
    ERR(fread(ents, sizeof(dir_ent_t), n_entries, fp), != n_entries,
        "Could not read %u IFD entries", n_entries);
    ifd->entries = ents;

    /* Next IFD offset */
    ERR(fread(&ifd->next_ifd, 1, 4, fp), != 4,
              "Could not read the address of the next IFD");
    return ifd;
}


/* Load TIFF metadata from disk */
static tiff_t *read_tiff(FILE *fp)
{
    ifd_t *last, *ifd;
    tiff_t *tiff;

    ERR(tiff = calloc(1, sizeof(tiff_t)), == NULL,
        "Not enough memory to store TIFF");

    /* Header */
    ERR(fread(&tiff->hdr, 1, sizeof(tiff_hdr_t), fp), != sizeof(tiff_hdr_t),
        "Could not read in the TIFF header");
    ERR(NATIVE2(tiff, tiff->hdr.universe), != 42,
        "Invalid TIFF magic number");

    /* Initial IFD */
    tiff->ifds = ifd = read_ifd(tiff, fp, NATIVE4(tiff, tiff->hdr.first_ifd));

    while (ifd->next_ifd != 0) {
        last = ifd;
        last->next = ifd = read_ifd(tiff, fp, NATIVE4(tiff, ifd->next_ifd));
    }

    return tiff;
}


/* Add a TIFF to the list of all tiffs */
static void add_tiff(tiff_t **head, tiff_t *tiff)
{
    tiff->next = *head;
    *head = tiff;
}


/* Open file and read in TIFF metadata */
static tiff_t *new_tiff(FILE *fp, const char *fname)
{
    tiff_t *tiff = read_tiff(fp);
    tiff->fname = fname;
    return tiff;
}


static void usage(const char *execname)
{
    printf("Usage: %s [-h] file.tiff...\n", execname);
    exit(EXIT_SUCCESS);
}


int main(int argc, char **argv)
{
    int opt;
    tiff_t *tiff, *tiffs = NULL;

    /* Args */
    while ((opt = getopt(argc, argv, "h:")) != -1) {
        switch (opt) {
        case 'h': 
        default:
            usage(argv[0]);
            break;
        }
    }

    /* Positional args (must be filenames) */
    while (optind < argc) {
        const char *fname = argv[optind++];
        FILE *fp = fopen(fname, "r");
        if (!fp) {
            fprintf(stderr, "Error(%d) opening \"%s\": %s\n",
                    errno, fname, strerror(errno));
            continue;
        }

        /* Valid file */
        add_tiff(&tiffs, new_tiff(fp, fname));
        fclose(fp);
    }

    /* Now do something with the TIFFs... */
    for (tiff=tiffs; tiff; tiff=tiff->next)
      locate_security_bits(tiff);

    return 0;
}
