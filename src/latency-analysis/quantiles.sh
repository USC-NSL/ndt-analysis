#! /bin/bash
#
# Assumes single column file with numerical values only

NUM_QUANTILES=1000
NUM_LINES=$(wc -l $1 | awk '{print $1}')
INTERVAL_X=$(bc -l <<< "$NUM_LINES / $NUM_QUANTILES")
INTERVAL_Y=$(bc -l <<< "1.0 / $NUM_QUANTILES")

sort -te -k2,2n -k1,1n $1 |\
    awk "BEGIN {
      next_x=$INTERVAL_X;
      next_y=0
      i=0;
    }
    { i=i+1;
      while (i >= next_x) {
          printf(\"%f,%f\n\", \$1, next_y);
          next_x=next_x+$INTERVAL_X;
          next_y=next_y+$INTERVAL_Y;
      }
    }"
