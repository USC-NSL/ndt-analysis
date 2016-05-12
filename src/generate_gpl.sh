#! /bin/bash
#
# Generates a PDF sequence graph for data packets flowing in one direction in
# the flow. This script works best with PCAP files that only store packets for a
# single flow each.
#
# Usage:
# generate_gpl.sh <PCAP file> <flow identifier> [<output file> <left boundary>
# <right boundary> ]
#
# Flow identifiers are assigned by the tcptrace tool, e.g. "a2b" identifying all
# packets flowing from endpoint A to endpoint B. If there are multiple flows
# they are assigned the next two available letters (e.g. "c2d"). The flow
# identifier specified here selects only packets from the corresponding
# connection and direction. For example, "b2a" selects all packets flowing from
# endpoint B to endpoint A (and their ACKs). For most traces these are the data
# packets transmitted by the server.
#
# Output is optional. If no value is provided the output filename is the same as
# the input filename with a ".pdf" appended
#
# Left and right boundary are optional and enable plotting only data showing up
# between the two specified time indexes (in seconds). 0 is marked by the first
# data packet from the selected endpoint.
#
# The produced plot shows:
# 1. Data packets (first transmits)
# 2. Data packets (retransmissions)
# 3. ACK progress (ignoring SACKs)
#
# Required tools: tcptrace, xpl2gpl, gnuplot
set -e

BASE_DIR=`dirname ${BASH_SOURCE[0]}`
SOURCE=$1
FLOW=$2
OUTPUT=$3
LEFT_BOUNDARY=$4
RIGHT_BOUNDARY=$5

[ "$OUTPUT" ] || OUTPUT="$SOURCE".pdf

shopt -s extglob

if [ "$(uname)" == "Darwin" ]; then
    TEMP_DIR=$(gmktemp -d)
else
    TEMP_DIR=$(mktemp -d)
fi
cp $SOURCE $TEMP_DIR/trace.pcap
cp $BASE_DIR/base.gpl $TEMP_DIR

cd $TEMP_DIR

# Generate plot source files
tcptrace -S --noshowsacks trace.pcap
mv ${FLOW}_tsg.xpl trace.xpl
xpl2gpl -s trace.xpl

TX_FILE=trace.dataset.white.uarrow
RTX_FILE=trace.dataset.red.uarrow

# Generate relative timestamps
START_TIME=$(head -1 $TX_FILE | awk '{print $1}')
START_SEQ=$(head -1 $TX_FILE | awk '{print $2}')

# Update scripts to use relative axis values
mv base.gpl trace.gpl
sed -i.bkp "s/\$\$NAME/trace/g" trace.gpl
sed -i.bkp "s/\$\$START_TIME/$START_TIME/g" trace.gpl
sed -i.bkp "s/\$\$START_SEQ/$START_SEQ/g" trace.gpl
if [[ $# -gt 3 ]]; then
    sed -i.bkp "s/\$\$RANGE_COMMAND/set xrange [$LEFT_BOUNDARY:$RIGHT_BOUNDARY]/g" trace.gpl
else
    sed -i.bkp "s/\$\$RANGE_COMMAND//g" trace.gpl
fi

# Generate sequence plot and overlay policing rate
gnuplot trace.gpl

cd -

cp $TEMP_DIR/trace.pdf $OUTPUT
rm -rf $TEMP_DIR
