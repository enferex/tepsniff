### tepsniff: TIFF/EPSniff looks for the SecurityClassification image tag.

#### What
TIFF/EP is the Tag Image File Format/Electronic Photography standard, which 
describes an image file format.  This format is a subclass of the TIFF standard that
 adds
multiple features, including a "SecurityClassification" tag.

The "SecurityClassification" tag semantically classifies the document (image) as
being of a particular information sensitivity. The value of the tag is based on
the [National Imagery Transmission Format](https://en.wikipedia.org/wiki/National_Imagery_Transmission_Format) 
(NITF) which follows the classification values defined in MIL-STD-2500.  The
value of the tag is a single character ASCII value T,S,C,R,U which represents
"Top Secret", "Secret", "Classified", "Restricted", or "Unclassified",
respectively.  tepsniff looks only for this tag.

#### Resources
* NITF - https://en.wikipedia.org/wiki/National_Imagery_Transmission_Format
* TIFF - https://en.wikipedia.org/wiki/TIFF
* TIFF/EP - http://www.digitalpreservation.gov/formats/content/tiff_tags.shtml 
* MIL-STD-2500C - http://www.gwg.nga.mil/ntb/baseline/docs/2500c/2500C.pdf

#### Build
Simply run `make` to build.

#### Contact
enferex: https://github.com/enferex
