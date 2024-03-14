#include "msocket.h"


int
m_socket (int domain, int type, int protocol)
{
    int retval = -1;

    int sizeSM = N * sizeof(struct SM);
    int sizeSOCK_INFO = sizeof(struct SOCK_INFO);

    int shmidSM = shm_open(KEY_SM, O_RDWR, 0);
    if (shmidSM == -1)
    {
        perror("shm_open");
        retval = -1;
    }

    struct SM *shmSM = mmap(0, sizeSM, PROT_READ | PROT_WRITE, MAP_SHARED, shmidSM, 0);
    if (shmSM == MAP_FAILED)
    {
        perror("mmap");
        retval = -1;
    }

    int shmidSOCK_INFO = shm_open(KEY_SOCK_INFO, O_RDWR, 0);
    if (shmidSOCK_INFO == -1)
    {
        perror("shm_open");
        retval = -1;
    }

    struct SOCK_INFO *shmSOCK_INFO = mmap(0, sizeSOCK_INFO, PROT_READ | PROT_WRITE, MAP_SHARED, shmidSOCK_INFO, 0);
    if (shmSOCK_INFO == MAP_FAILED)
    {
        perror("mmap");
        retval = errno;
    }

    shmSOCK_INFO->sockfd = 0;
    memset(&shmSOCK_INFO->addr, 0, sizeof(struct sockaddr_in));
    shmSOCK_INFO->err = 0;

    int i;
    for (i = 0; i < N; i++)
    {
        if (shmSM[i].isFree)
        {
            shmSM[i].isFree = 0;
            shmSM[i].pid = getpid();
            logger(LOGFILE, "msocket.c 51: Free slot found at index %d", i);
            break;
        }
    }

    if (i == N)
    {
        retval = -1;
        errno = ENOBUFS;
    }

    if (sem_post(&shmSOCK_INFO->sem1) == -1)
    {
        perror("sem_post");
        retval = -1;
    }

    if (sem_wait(&shmSOCK_INFO->sem2) == -1)
    {
        perror("sem_wait");
        retval = -1;
    }

    shmSM[i].UDPfd = shmSOCK_INFO->sockfd;
    retval = shmSOCK_INFO->sockfd;

    if (munmap(shmSM, sizeSM) == -1)
    {
        perror("munmap");
        retval = -1;
    }

    if (munmap(shmSOCK_INFO, sizeSOCK_INFO) == -1)
    {
        perror("munmap");
        retval = -1;
    }

    close(shmidSM);
    close(shmidSOCK_INFO);

    if (retval != 0)
    {
        logger(LOGFILE, "msocket.c 94: Error creating socket: %s", strerror(errno));
    }

    return retval;
}

int
m_bind (int sockfd, const struct sockaddr *srcaddr, socklen_t srcaddrlen, const struct sockaddr *destaddr, socklen_t destaddrlen)
{
    int retval = 0;

    int sizeSM = N * sizeof(struct SM);
    int sizeSOCK_INFO = sizeof(struct SOCK_INFO);

    int shmidSM = shm_open(KEY_SM, O_RDWR, 0);
    if (shmidSM == -1)
    {
        perror("shm_open");
        retval = -1;
    }

    struct SM *shmSM = mmap(0, sizeSM, PROT_READ | PROT_WRITE, MAP_SHARED, shmidSM, 0);
    if (shmSM == MAP_FAILED)
    {
        perror("mmap");
        retval = -1;
    }

    int shmidSOCK_INFO = shm_open(KEY_SOCK_INFO, O_RDWR, 0);
    if (shmidSOCK_INFO == -1)
    {
        perror("shm_open");
        retval = -1;
    }

    struct SOCK_INFO *shmSOCK_INFO = mmap(0, sizeSOCK_INFO, PROT_READ | PROT_WRITE, MAP_SHARED, shmidSOCK_INFO, 0);
    if (shmSOCK_INFO == MAP_FAILED)
    {
        perror("mmap");
        retval = -1;
    }

    shmSOCK_INFO->sockfd = sockfd;
    struct sockaddr_in *saddr = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in));
    *saddr = *(struct sockaddr_in *)srcaddr;

    shmSOCK_INFO->addr = *saddr;

    if (sem_post(&shmSOCK_INFO->sem1) == -1)
    {
        perror("sem_post");
        retval = -1;
    }

    if (sem_wait(&shmSOCK_INFO->sem2) == -1)
    {
        perror("sem_wait");
        retval = -1;
    }

    for (int i = 0; i < N; i++)
    {
        if (shmSM[i].UDPfd == sockfd)
        {
            struct sockaddr_in *daddr = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in));
            *daddr = *(struct sockaddr_in *)destaddr;
            shmSM[i].addr = *daddr;
            break;
        }
    }

    if (munmap(shmSM, sizeSM) == -1)
    {
        perror("munmap");
        retval = -1;
    }

    if (munmap(shmSOCK_INFO, sizeSOCK_INFO) == -1)
    {
        perror("munmap");
        retval = -1;
    }

    close(shmidSM);
    close(shmidSOCK_INFO);

    if (retval != 0)
    {
        logger(LOGFILE, "msocket.c 94: Error binding socket: %s", strerror(errno));
    }

    return retval;
}

int
m_sendto (int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen)
{
    int retval = 0;

    int sizeSM = N * sizeof(struct SM);

    // msg format: MSG<seq><msg>POSTAMBLE

    char msg[1024];
    memset(msg, 0, 1024);

    int shmidSM = shm_open(KEY_SM, O_CREAT | O_RDWR, 0666);
    if (shmidSM == -1)
    {
        perror("shm_open");
        retval = -1;
    }

    struct SM *shmSM = mmap(0, sizeSM, PROT_READ | PROT_WRITE, MAP_SHARED, shmidSM, 0);
    if (shmSM == MAP_FAILED)
    {
        perror("mmap");
        retval = -1;
    }

    int i;
    for (i = 0; i < N; i++)
    {
        if (shmSM[i].UDPfd == sockfd)
        {
            struct sockaddr *shmSMaddr = (struct sockaddr *)malloc(addrlen);
            *shmSMaddr = *(struct sockaddr *)(&shmSM[i].addr);

            if (memcmp(dest_addr, shmSMaddr, addrlen) == 0)
            {
                if (shmSM[i].sbuff[shmSM[i].lastPut][0] == '\0')
                {
                    char seq[SEQ_LEN + 1];
                    if (shmSM[i].currSeq < 10)
                    {
                        sprintf(seq, "0%d", shmSM[i].currSeq);
                    }
                    else
                    {
                        sprintf(seq, "%d", shmSM[i].currSeq);
                    }
                    strcat(msg, MSG);
                    strcat(msg, seq);
                    memcpy(msg + strlen(MSG) + SEQ_LEN, (char *)buf, len);
                    strcat(msg, POSTAMBLE);
                    strcpy(shmSM[i].sbuff[shmSM[i].lastPut], msg);
                    
                    logger(LOGFILE, "msocket.c 238: Putting message %s in send buffer at index %d", shmSM[i].sbuff[shmSM[i].lastPut], shmSM[i].lastPut);
                    
                    shmSM[i].currSeq = (shmSM[i].currSeq + 1) % 16;
                    shmSM[i].lastPut = (shmSM[i].lastPut + 1) % 10;

                    break;
                }
                else
                {
                    retval = -1;
                    errno = ENOBUFS;
                }
            }
            else
            {
                retval = -1;
                errno = ENOTCONN;
            }
            break;
        }
    }

    if (i == N)
    {
        retval = -1;
        errno = ENOTCONN;
    }

    if (munmap(shmSM, sizeSM) == -1)
    {
        perror("munmap");
        retval = -1;
    }

    close(shmidSM);

    if (retval != 0)
    {
        logger(LOGFILE, "msocket.c 279: Error sending message: %s", strerror(errno));
    }

    return retval;
}


int
m_recvfrom (int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen)
{
    int retval = 0;

    int sizeSM = N * sizeof(struct SM);

    int shmidSM = shm_open(KEY_SM, O_CREAT | O_RDWR, 0666);
    if (shmidSM == -1)
    {
        perror("shm_open");
        retval = errno;
    }

    struct SM *shmSM = mmap(0, sizeSM, PROT_READ | PROT_WRITE, MAP_SHARED, shmidSM, 0);
    if (shmSM == MAP_FAILED)
    {
        perror("mmap");
        retval = errno;
    }
    
    int i;
    for (i = 0; i < N; i++)
    {
        if (shmSM[i].UDPfd == sockfd)
        {
            int j;
            for (j = (shmSM[i].rwnd.base + shmSM[i].rwnd.size) % 5; j != shmSM[i].rwnd.base; j = (j + 1) % 5)
            {
                if (shmSM[i].rbuff[j][0] != '\0')
                {
                    retval = strlen(shmSM[i].rbuff[j]) - strlen(MSG) - SEQ_LEN - strlen(POSTAMBLE);
                    memcpy(buf, shmSM[i].rbuff[j] + strlen(MSG) + SEQ_LEN, retval);

                    logger(LOGFILE, "msocket.c 314: Received message %s from receive buffer at index %d", shmSM[i].rbuff[j], j);
                    break;
                }
            }
            if (j == shmSM[i].rwnd.base)
            {
                retval = -1;
                errno = ENOMSG;
            }

            break;
        }
    }

    if (i == N)
    {
        retval = -1;
        errno = ENOTCONN;
    }

    if (munmap(shmSM, sizeSM) == -1)
    {
        perror("munmap");
        retval = -1;
    }

    close(shmidSM);

    if (retval != 0)
    {
        logger(LOGFILE, "msocket.c 350: Error receiving message: %s", strerror(errno));
    }

    return retval;
}


int
m_close (int sockfd)
{
    int retval = 0;

    int sizeSM = N * sizeof(struct SM);
    int sizeSOCK_INFO = sizeof(struct SOCK_INFO);

    int shmidSM = shm_open(KEY_SM, O_CREAT | O_RDWR, 0666);
    if (shmidSM == -1)
    {
        perror("shm_open");
        retval = errno;
    }

    struct SM *shmSM = mmap(0, sizeSM, PROT_READ | PROT_WRITE, MAP_SHARED, shmidSM, 0);
    if (shmSM == MAP_FAILED)
    {
        perror("mmap");
        retval = errno;
    }

    int shmidSOCK_INFO = shm_open(KEY_SOCK_INFO, O_CREAT | O_RDWR, 0666);
    if (shmidSOCK_INFO == -1)
    {
        perror("shm_open");
        retval = errno;
    }

    struct SOCK_INFO *shmSOCK_INFO = mmap(0, sizeSOCK_INFO, PROT_READ | PROT_WRITE, MAP_SHARED, shmidSOCK_INFO, 0);
    if (shmSOCK_INFO == MAP_FAILED)
    {
        perror("mmap");
        retval = errno;
    }

    memset(&shmSOCK_INFO->addr, 0, sizeof(struct sockaddr_in));
    shmSOCK_INFO->err = 0;
    shmSOCK_INFO->sockfd = sockfd;

    int i;
    for (i = 0; i < N; i++)
    {
        if (shmSM[i].UDPfd == sockfd)
        {
            shmSM[i].isFree = 1;
            shmSM[i].pid = 0;
            break;
        }
    }

    if (i == N)
    {
        retval = -1;
        errno = ENOTCONN;
    }

    if (sem_post(&shmSOCK_INFO->sem1) == -1)
    {
        perror("sem_post");
        retval = -1;
    }

    if (sem_wait(&shmSOCK_INFO->sem2) == -1)
    {
        perror("sem_wait");
        retval = -1;
    }

    retval = shmSOCK_INFO->err;

    if (munmap(shmSM, sizeSM) == -1)
    {
        perror("munmap");
        retval = -1;
    }

    if (munmap(shmSOCK_INFO, sizeSOCK_INFO) == -1)
    {
        perror("munmap");
        retval = -1;
    }

    close(shmidSM);
    close(shmidSOCK_INFO);

    if (retval != 0)
    {
        logger(LOGFILE, "msocket.c 94: Error closing socket: %s", strerror(errno));
    }

    return retval;
}


int
dropMessage(float p)
{
    return (rand() < p * RAND_MAX);
}


int
logger (char *fname, const char *format, ...)
{
    // Get current time
    time_t now = time(NULL);
    char timestamp[40];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));

    // Open log file
    FILE *log = fopen(fname, "a");
    if (log == NULL)
    {
        perror("fopen");
        return -1;
    }
    

    // Write log message
    va_list args;
    va_start(args, format);
    fprintf(log, "%s ", timestamp);
    vfprintf(log, format, args);
    fprintf(log, "\n");
    va_end(args);

    // Close log file
    fclose(log);

    return 0;
}
