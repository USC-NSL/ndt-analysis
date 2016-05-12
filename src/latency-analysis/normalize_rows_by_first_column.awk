#! /usr/bin/awk -F, -f

{
    div = $1;
    if (div == 0) {
        div = 1;
    }
    for (i = 1; i < NF; i++) {
        printf "%f,", ($i / div);
    }
    printf "%f\n", ($NF / div);
}
