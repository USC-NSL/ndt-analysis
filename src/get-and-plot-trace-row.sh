#! /bin/bash
#
# Fetches the NDT trace associated with the Nth row in the input file
# and generates the sequence graph for the given direction

set -e

INPUT_FILE=$1
ROW_INDEX=$2

BASE_PATH="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# Extract the GS and trace file descriptors
read GS_FILE TRACE_FILE DIRECTION <<< $(sed "${ROW_INDEX} q;d" $INPUT_FILE |\
    awk -F, '{print $1 " " $2 " " $4}')

# Fetch the trace file (and decompress if necessary)
bash $BASE_PATH/get-matching-ndt-file.sh $GS_FILE $TRACE_FILE
if file $TRACE_FILE | grep 'gzip compressed data' > /dev/null; then
    mv $TRACE_FILE $TRACE_FILE.gz
    gunzip $TRACE_FILE.gz
fi

# Plot the trace file
bash $BASE_PATH/generate_gpl.sh $TRACE_FILE $DIRECTION
