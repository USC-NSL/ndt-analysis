#!/bin/bash

set -e

UNAME_S=$(uname -s)
if [ "$UNAME_S" == "Darwin" ]; then
    READLINK=greadlink
else
    READLINK=readlink
fi
export GLOG_logtostderr=1
PROCESS_PCAP="`$READLINK -f $(dirname \"${BASH_SOURCE[0]}\")/analyze_latency`"

CSV_FILE=$1
GS_FILE=`echo $CSV_FILE | sed -e 's#^\(\(....\)\(..\)\(..\).*\)\.csv$#gs://m-lab/ndt/\2/\3/\4/\1.tgz#'`

echo "Processing $GS_FILE"

TEMP_DIR=`mktemp -d /tmp/tmp.XXXXXX`

gsutil cp $GS_FILE $TEMP_DIR

tar xzf $TEMP_DIR/*.tgz --strip=3 -C $TEMP_DIR
rm -f $TEMP_DIR/*.c2s_ndttrace $TEMP_DIR/*.tgz

# Check if there are any files to process.
if ls $TEMP_DIR/*ndttrace >/dev/null 2>&1; then
  cd $TEMP_DIR
  for TRACE in `ls -1 *ndttrace`; do
    if file $TRACE | grep 'gzip compressed data' > /dev/null; then
      mv $TRACE $TRACE.gz
      gunzip $TRACE.gz
    fi
    ulimit -Sv 100000000
    echo "Trace: $TEMP_DIR/$TRACE"
    mv $TRACE $TRACE.bkp
    reordercap $TRACE.bkp $TRACE || continue
    ($PROCESS_PCAP $TRACE || echo $TRACE,ERROR) | sed -e "s#^#$GS_FILE,#" >> result.csv
  done
  cd -

  cp $TEMP_DIR/result.csv $CSV_FILE
else
  echo "$GS_FILE has no trace data."
  touch $CSV_FILE
fi

rm -rf $TEMP_DIR
