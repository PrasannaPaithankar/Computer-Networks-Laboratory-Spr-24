#include "msocket.h"

int
m_socket (int domain, int type, int protocol)
{
    int retval;

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

    // check for free socket
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
m_bind (int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    int retval;

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
    shmSOCK_INFO->addr = *(struct sockaddr_in *)addr;

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
    fprintf(log, "%s [%s] ", timestamp);
    vfprintf(log, format, args);
    fprintf(log, "\n");
    va_end(args);

    // Close log file
    fclose(log);

    return 0;
}


int
dropMessage(float p)
{
    return (rand() < p * RAND_MAX);
}

