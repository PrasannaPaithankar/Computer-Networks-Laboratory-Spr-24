#include "msocket.h"

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
    dest_addr.sin_addr.s_addr = inet_addr(argv[3]);

    retval = m_bind(M1, (struct sockaddr *)&src_addr, sizeof(src_addr), (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (retval == -1)
    {
        perror("m_bind");
        return errno;
    }

    printf("Socket bound to port %d\n", atoi(argv[1]));

    int fd = open("file.txt", O_RDONLY);
    if (fd == -1)
    {
        perror("open");
        return errno;
    }

    char buf[MAXBUFLEN];
    int n;
    while ((n = read(fd, buf, MAXBUFLEN)) > 0)
    {
        retval = m_sendto(M1, buf, n, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (retval == -1)
        {
            perror("m_sendto");
            return errno;
        }
        memset(buf, 0, MAXBUFLEN);
    }

    close(fd);
    m_close(M1);    

    printf("File sent\n");
    
    return 0;
}