#!/bin/bash
#
# Fetches a NDT trace file based on the first two fields of the input
# line with the following format:
# <archive file> <trace file> <target file>

set -e

GS_FILE=$1
TRACE_FILE=$2

if [ "$(uname)" == "Darwin" ]; then
    MKTEMP=gmktemp
else
    MKTEMP=mktemp
fi

if [ $# -ge 4 ]; then
    TARGET_FILE=$3
else
    TARGET_FILE=$TRACE_FILE
fi

echo "Archive: $GS_FILE"
echo "Trace: $TRACE_FILE"
echo "Target: $TARGET_FILE"


TEMP_DIR=`$MKTEMP -d /tmp/tmp.XXXXXX`

gsutil cp $GS_FILE $TEMP_DIR

tar xzf $TEMP_DIR/*.tgz --strip=3 -C $TEMP_DIR
if ls $TEMP_DIR/$TRACE_FILE >/dev/null 2>&1; then
    cd $TEMP_DIR 
    if file $TRACE_FILE | grep 'gzip compressed data' > /dev/null; then
        mv $TRACE_FILE ${TRACE_FILE}.gz
        gunzip ${TRACE_FILE}.gz
    fi
    cd -
    mv $TEMP_DIR/$TRACE_FILE $TARGET_FILE
    echo "Extracted $TRACE_FILE, and saved as $TARGET_FILE"
else
    echo "$TRACE_FILE does not exist in archive $GS_FILE"
fi
rm -rf $TEMP_DIR
