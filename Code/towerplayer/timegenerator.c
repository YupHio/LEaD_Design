#include <stdio.h>
#include <stdlib.h>
int main(int argc, char** argv) {
    int numReps = atoi(argv[1]);
    int x, y, z;
    char * blankRow = "0 0 0 0 0 0 0 0 0 0 0 0 \n";
    char * red = "255 0 0";
    char * green = "0 255 0";
    char * blue = "0 0 255";
    char * yellow = "255 255 0";
    char * chan0 = red;
    char * chan1 = green;
    char * chan2 = blue;
    char * chan3 = yellow;
    char * temp;
    int minutes = 0;
    int seconds = 0;
    for (x = 0; x < numReps; x++) {
        for (y = 0; y < 4; y++) {
            fprintf(stdout, "%.2i:%.2i.000\n", minutes, seconds);
            seconds++;
            if (seconds == 60) {
                seconds = 0;
                minutes++;
            }
            fprintf(stdout, "%s %s %s %s \n", chan0, chan1, chan2, chan3);
            fprintf(stdout, "%s %s %s %s \n", chan0, chan1, chan2, chan3);
            for (z = 0; z < 8; z++){
                fprintf(stdout, "%s %s %s %s \n", chan0, chan1, chan2, chan3);
                //fprintf(stdout, "%s", blankRow);
            }
            temp = chan0;
            chan0 = chan1;
            chan1 = chan2;
            chan2 = chan3;
            chan3 = temp;
        }
    }
}
