#! /usr/bin/awk -F, -f

{
    sum = 0;
    for (i = 1; i <= NF; i++) {
        sum += $i;
    }
    if (sum == 0) {
        sum = 1;
    }
    for (i = 1; i < NF; i++) {
        printf "%f,", ($i / sum);
    }
    printf "%f\n", ($NF / sum);
}
