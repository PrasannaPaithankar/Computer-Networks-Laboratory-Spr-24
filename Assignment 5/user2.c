/*
 *  Author:     Prasanna Paithankar
 *  Roll No.:   21CS30065
 *  Course:     Computer Networks Laboratory (CS39006) Spr 2023-24
 *  Date:       15/03/2024

 *  File:       user1.c
 *  Compile:    gcc -o user2 user2.c -L. -lmsocket
 *  Run:        ./user2 <src_port> <dest_port> <dest_ip>
 */

#include "msocket.h"

int main(int argc, char *argv[])
{
    int M2;
    struct sockaddr_in src_addr, dest_addr;
    int retval;

    M2 = m_socket(AF_INET, SOCK_MTP, 0);
    if (M2 == -1)
    {
        perror("m_socket");
        return errno;
    }
    printf("Socket created\n");

    src_addr.sin_family = AF_INET;
    src_addr.sin_port = htons(atoi(argv[1]));
    src_addr.sin_addr.s_addr = INADDR_ANY;

    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(atoi(argv[2]));
    inet_pton(AF_INET, argv[3], &dest_addr.sin_addr);

    retval = m_bind(M2, (struct sockaddr *)&src_addr, sizeof(src_addr), (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (retval == -1)
    {
        perror("m_bind");
        return errno;
    }

    printf("Socket bound to port %d\n", atoi(argv[1]));

    FILE *fp = fopen("file2.txt", "w");
    if (fp == NULL)
    {
        perror("fopen");
        return errno;
    }

    printf("File opened\n");

    char buf[MAXBUFLEN];
    memset(buf, 0, MAXBUFLEN);
    int n;
    while (1)
    {
        n = -1;
        while (n == -1)
        {
            sleep(2);
            // printf("Waiting for data...\n");
            n = m_recvfrom(M2, buf, MAXBUFLEN, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
            if (errno == ENOMSG)
            {
                continue;
            }
            else
            {
                perror("m_recvfrom");
                return errno;
            }
        } 
        if (n == 0)
        {
            break;
        }
        
        printf("%s", buf);
        fflush(stdout);
        fprintf(fp, "%s", buf);
        fflush(fp);

        memset(buf, 0, MAXBUFLEN);
    }

    fclose(fp);
    m_close(M2);

    printf("File received\n");

    return 0;
}
