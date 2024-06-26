/*
 *  Author:     Prasanna Paithankar
 *  Roll No.:   21CS30065
 *  Course:     Computer Networks Laboratory (CS39006) Spr 2023-24
 *  Date:       15/03/2024

 *  File:       user1.c
 *  Compile:    gcc -o user1 user1.c -L. -lmsocket
 *  Run:        ./user1 <src_port> <dest_port> <dest_ip>
 */

#include "../include/msocket.h"
// #include <msocket.h>

int main(int argc, char *argv[])
{
    int M1;
    struct sockaddr_in src_addr, dest_addr;
    int retval;

    M1 = m_socket(AF_INET, SOCK_MTP, 0);
    if (M1 == -1)
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

    retval = m_bind(M1, (struct sockaddr *)&src_addr, sizeof(src_addr), (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (retval == -1)
    {
        perror("m_bind");
        return errno;
    }

    printf("Socket bound to port %d\n", atoi(argv[1]));

    int fd = open("./test/selfpotrait.jpg", O_RDONLY);
    if (fd == -1)
    {
        perror("open");
        return errno;
    }

    // printf size of the file
    struct stat st;
    fstat(fd, &st);
    printf("Size of the file: %ld\n", st.st_size);

    char buf[MAXBUFLEN - 10];
    int n;
    while ((n = read(fd, buf, MAXBUFLEN - 10)) > 0)
    {
        while (m_sendto(M1, buf, n, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) == -1)
        {
            if (errno == ENOBUFS)
            {
                continue;
            }
            else
            {
                perror("m_sendto");
                return errno;
            }
        }
        memset(buf, 0, MAXBUFLEN - 10);   
        sleep(5);
    }

    close(fd);
    m_close(M1);    

    printf("File sent\n");
    
    return 0;
}