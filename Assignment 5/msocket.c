#include "msocket.h"

int
m_socket (int domain, int type, int protocol)
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

    memset(shmSOCK_INFO, 0, sizeSOCK_INFO);

    int i;
    for (i = 0; i < N; i++)
    {
        if (shmSM[i].isFree)
        {
            shmSM[i].isFree = 0;
            shmSM[i].pid = getpid();
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
        retval = errno;
    }

    if (sem_wait(&shmSOCK_INFO->sem2) == -1)
    {
        perror("sem_wait");
        retval = errno;
    }

    shmSM[i].UDPfd = shmSOCK_INFO->sockfd;
    retval = shmSOCK_INFO->err;

    if (shm_unlink(KEY_SM) == -1)
    {
        perror("shm_unlink");
        retval = errno;
    }

    if (shm_unlink(KEY_SOCK_INFO) == -1)
    {
        perror("shm_unlink");
        retval = errno;
    }

    return retval;
}

int
m_bind (int sockfd, const struct sockaddr *srcaddr, socklen_t srcaddrlen, const struct sockaddr *destaddr, socklen_t destaddrlen)
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

    memset(shmSOCK_INFO, 0, sizeSOCK_INFO);

    shmSOCK_INFO->sockfd = sockfd;
    shmSOCK_INFO->addr = *(struct sockaddr_in *)srcaddr;

    if (sem_post(&shmSOCK_INFO->sem1) == -1)
    {
        perror("sem_post");
        retval = errno;
    }

    if (sem_wait(&shmSOCK_INFO->sem2) == -1)
    {
        perror("sem_wait");
        retval = errno;
    }

    for (int i = 0; i < N; i++)
    {
        if (shmSM[i].UDPfd == sockfd)
        {
            shmSM[i].addr = *(struct sockaddr_in *)destaddr;
            break;
        }
    }

    retval = shmSOCK_INFO->err;

    if (shm_unlink(KEY_SM) == -1)
    {
        perror("shm_unlink");
        retval = errno;
    }

    if (shm_unlink(KEY_SOCK_INFO) == -1)
    {
        perror("shm_unlink");
        retval = errno;
    }

    return retval;
}

int
m_sendto (int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen)
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
            if (memcmp(&shmSM[i].addr, dest_addr, addrlen) == 0)
            {
                int j;
                for (j = 0; j < 10; j++)
                {
                    if (shmSM[i].sbuff[j][0] == '\0')
                    {
                        strcpy(shmSM[i].sbuff[j], buf);
                        break;
                    }
                }
                if (j == 10)
                {
                    retval = -1;
                    errno = ENOBUFS;
                }
            }
            else
            {
                retval = -1;
                errno = ENOTBOUND;
            }
            break;
        }
    }

    if (i == N)
    {
        retval = -1;
        errno = ENOTBOUND;
    }

    if (shm_unlink(KEY_SM) == -1)
    {
        perror("shm_unlink");
        retval = errno;
    }

    return retval;
}


int
m_recvfrom (int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen)
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
    
    int i;
    for (i = 0; i < N; i++)
    {
        if (shmSM[i].UDPfd == sockfd)
        {
            int j;
            for (j = 0; j < 10; j++)
            {
                if (shmSM[i].rbuff[j][0] != '\0')
                {
                    strcpy(buf, shmSM[i].rbuff[j]);
                    shmSM[i].rbuff[j][0] = '\0';
                    break;
                }
            }
            if (j == 10)
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
        errno = ENOTBOUND;
    }

    if (shm_unlink(KEY_SM) == -1)
    {
        perror("shm_unlink");
        retval = errno;
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

    memset(shmSOCK_INFO, 0, sizeSOCK_INFO);
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
        errno = ENOTBOUND;
    }

    if (sem_post(&shmSOCK_INFO->sem1) == -1)
    {
        perror("sem_post");
        retval = errno;
    }

    if (sem_wait(&shmSOCK_INFO->sem2) == -1)
    {
        perror("sem_wait");
        retval = errno;
    }

    retval = shmSOCK_INFO->err;

    if (shm_unlink(KEY_SM) == -1)
    {
        perror("shm_unlink");
        retval = errno;
    }

    if (shm_unlink(KEY_SOCK_INFO) == -1)
    {
        perror("shm_unlink");
        retval = errno;
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
