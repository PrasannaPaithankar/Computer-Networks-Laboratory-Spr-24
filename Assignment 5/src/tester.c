#include "../include/msocket.h"

FILE *file;

int signalHandler(int signum)
{
    fclose(file);
    write(1, "Exiting cleanly\n", 16);
    exit(0);
}

int main(int argc, char *argv[])
{
    int data[2];
    float p = atof(argv[2]);

    signal(SIGINT, signalHandler);

    // Open file argv[1] for reading
    file = fopen(argv[1], "w");
    if (file == NULL)
    {
        printf("Error: File not found\n");
        return 0;
    }

    // Do initial fetch
    while (1)
    {
        if (fetchTest(p, data) == -1)
        {
            printf("Error: Fetch failed\n");
            return 0;
        }
        fprintf(file, "%d,%d\n", data[0], data[1]);
        printf("%d,%d\n", data[0], data[1]);
        fflush(file);
        fflush(stdout);
        sleep(2*T);
    }

    fclose(file);
    return 0;
}
