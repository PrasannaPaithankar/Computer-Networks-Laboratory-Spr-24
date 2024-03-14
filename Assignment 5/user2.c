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

    int fd = open("file2.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1)
    {
        perror("open");
        return errno;
    }

    printf("File opened\n");

    char buf[MAXBUFLEN];
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
        
        retval = write(fd, buf, n);
        if (retval == -1)
        {
            perror("write");
            return errno;
        }
        memset(buf, 0, MAXBUFLEN);
    }

    close(fd);
    m_close(M2);

    printf("File received\n");

    return 0;
}
