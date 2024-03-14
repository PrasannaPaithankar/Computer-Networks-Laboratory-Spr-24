#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <time.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/epoll.h>
#include <stdarg.h>

#ifndef MSOCKETLIB
#define MSOCKETLIB

    #define SOCK_MTP SOCK_DGRAM

    #define MAXBUFLEN 1024

    #define LOGFILE "log.txt"

    #define T 5
    #define N 25

    #define MSG "MSG"
    #define ACK "ACK"
    #define SEQ_LEN 2
    #define POSTAMBLE "END"

    #define KEY_SM "pipiSM"
    #define KEY_SOCK_INFO "pipiSOCK_INFO"

#endif


struct window
{
    int                 size;                       // for swnd: max number of messages that can be sent w/o ack; for rwnd: max number of messages that can be received
    int                 base;                       // for swnd: sequence number of the first message in the window; for rwnd: sequence number of the first message expected to be received
    time_t              timestamp[10];              // for swnd: contains the time at which the message was sent; for rwnd: contains the time at which the message was received
};

struct SM
{
    int                 isFree;             // 0: allocated, 1: free
    pid_t               pid;                // PID of the process that created the socket
    int                 UDPfd;              // UDP socket file descriptor
    struct sockaddr_in  addr;               // Address of the other end
    char                sbuff[10][1024];    // Send buffer
    char                rbuff[10][1024];    // Receive buffer
    struct window       swnd;               // Send window
    int                 currSeq;            // Current sequence number
    struct window       rwnd;               // Receive window
    int                 currExpSeq;         // Current expected sequence number
    int                 lastAck;            // Last acknowledged sequence number
};

struct SOCK_INFO
{
    sem_t               sem1;       // Semaphore 1
    sem_t               sem2;       // Semaphore 2
    int                 sockfd;     // Socket file descriptor
    struct sockaddr_in  addr;       // Address of this machine
    int                 err;        // Error code
};


int
m_socket(int domain, int type, int protocol);

int
m_bind(int sockfd, const struct sockaddr *srcaddr, socklen_t srcaddrlen, const struct sockaddr *destaddr, socklen_t destaddrlen);

int
m_sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen);

int
m_recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen);

int
m_close(int sockfd);

int
dropMessage(float p);

int
logger(char *fname, const char *format, ...);
