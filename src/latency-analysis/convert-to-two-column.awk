#! /bin/awk -f
# Treat each input line as a collection of
# (x,y) pairs and convert them to two-column format
#
# Example:
#
# 1,2,3,4,5,6
#
# ->
#
# 1,2
# 3,4
# 5,6
{
    for (i = 1; i <= NF; i = i + 2) {
        j = i + 1;
        printf "%s,%s\n", $i, $j;
	}
}
