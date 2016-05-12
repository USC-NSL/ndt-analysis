#! /bin/bash
#
# Assumes two-column input file, and when selecting all rows
# with for a particular key (first column), all values in the
# second column are already ordered

UNAME_S=$(uname -s)
if [ "$UNAME_S" == "Darwin" ]; then
    READLINK=greadlink
else
    READLINK=readlink
fi

QUANTILES_SH="`$READLINK -f $(dirname \"${BASH_SOURCE[0]}\")/quantiles.sh`"

TEMP_DIR=`mktemp -d /tmp/tmp.XXXXXX`
TEMP_FILE_KEY=$TEMP_DIR/.key
TEMP_FILE_OUT=$TEMP_DIR/.out
TEMP_FILE_MERGED=$TEMP_DIR/.merged

touch $TEMP_FILE_MERGED
for key in `awk -F, '{print $1}' $1 | sort -n | uniq | xargs`; do
    awk -F, "{if (\$1 == $key) print \$2}" $1 > $TEMP_FILE_KEY
    $QUANTILES_SH $TEMP_FILE_KEY | paste -d, $TEMP_FILE_MERGED - > $TEMP_FILE_OUT
    mv $TEMP_FILE_OUT $TEMP_FILE_MERGED
done

cut -c 2- $TEMP_FILE_MERGED
rm -rf $TEMP_DIR
