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
#include <sys/select.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/epoll.h>
#include <sys/poll.h>
#include <sys/uio.h>
#include <sys/utsname.h>
#include <sys/resource.h>
#include <sys/ipc.h>
#include <stdarg.h>

#ifndef MAXBUFLEN
#define MAXBUFLEN 1024
#endif

#ifndef LOGFILE
#define LOGFILE "log.txt"
#endif

#ifndef T
#define T 5
#endif

#ifndef N
#define N 25
#endif

#ifndef FORMAT
#define FORMAT
#define MSG "MSG"
#define ACK "ACK"
#define POSTAMBLE "END"
#endif

#ifndef KEYS
#define KEY_SM "pipiSM"
#define KEY_SOCK_INFO "pipiSOCK_INFO"
#endif


struct window
{
    int     size;
    int     acked[16];
};

struct SM
{
    int                 isFree; // 0: allocated, 1: free
    pid_t               pid;    // PID of the process that created the socket
    int                 UDPfd;  // UDP socket file descriptor
    struct sockaddr_in  addr;   // Address of the other end
    char sbuff[10][1024];       // Send buffer
    char rbuff[10][1024];       // Receive buffer
    struct window *swnd;        // Send window
    struct window *rwnd;        // Receive window
};

struct SOCK_INFO
{
    sem_t               sem1;
    sem_t               sem2;
    int                 sockfd;
    struct sockaddr_in  addr;
    int                 err;
};

int
logger(char *fname, const char *format, ...);

int
m_socket();

int
m_sendto();

int
m_recvfrom();

int
m_close();

int
dropMessage(float p);
