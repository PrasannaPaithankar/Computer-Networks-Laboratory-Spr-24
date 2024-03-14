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
    src_addr.sin_port = htons(20001);
    src_addr.sin_addr.s_addr = INADDR_ANY;

    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(20000);
    inet_pton(AF_INET, "127.0.0.1", &dest_addr.sin_addr);

    retval = m_bind(M1, (struct sockaddr *)&src_addr, sizeof(src_addr), (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (retval == -1)
    {
        perror("m_bind");
        return errno;
    }

    printf("Socket bound to port %d\n", 20001);

    m_close(M1);    

    printf("Done\n");
    
    return 0;
}