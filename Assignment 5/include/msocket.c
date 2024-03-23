/*
 *  Author:     Prasanna Paithankar
 *  Roll No.:   21CS30065
 *  Course:     Computer Networks Laboratory (CS39006) Spr 2023-24
 *  Date:       15/03/2024

 *  File:       msocket.c
 *  Purpose:    Implementation of the msocket library
 */

#include "msocket.h"


/*
 *  Function:   m_socket
 *  ---------------------
 *  Description:    Create a new MTP socket
 *  Parameters:     domain = Address family (AF_INET)
 *                  type = Socket type (SOCK_MTP)
 *                  protocol = Protocol (0)
 *  Returns:        int:        File descriptor of the new MTP socket
 *  Errors:         ENOBUFS:    No buffer space available  
 */
int
m_socket (int domain, int type, int protocol)
{
    int retval = -1;

    int sizeSM = N * sizeof(struct SM);
    int sizeSOCK_INFO = sizeof(struct SOCK_INFO);

    /* Create shared memory for SM */
    int shmidSM = shm_open(KEY_SM, O_RDWR, 0);
    if (shmidSM == -1)
    {
        perror("shm_open");
        retval = -1;
    }

    /* Map shared memory for SM */
    struct SM *shmSM = mmap(0, sizeSM, PROT_READ | PROT_WRITE, MAP_SHARED, shmidSM, 0);
    if (shmSM == MAP_FAILED)
    {
        perror("mmap");
        retval = -1;
    }

    /* Create shared memory for SOCK_INFO */
    int shmidSOCK_INFO = shm_open(KEY_SOCK_INFO, O_RDWR, 0);
    if (shmidSOCK_INFO == -1)
    {
        perror("shm_open");
        retval = -1;
    }

    /* Map shared memory for SOCK_INFO */
    struct SOCK_INFO *shmSOCK_INFO = mmap(0, sizeSOCK_INFO, PROT_READ | PROT_WRITE, MAP_SHARED, shmidSOCK_INFO, 0);
    if (shmSOCK_INFO == MAP_FAILED)
    {
        perror("mmap");
        retval = errno;
    }

    shmSOCK_INFO->sockfd = 0;
    memset(&shmSOCK_INFO->addr, 0, sizeof(struct sockaddr_in));
    shmSOCK_INFO->err = 0;

    /* Find a free slot in SM */
    int i;
    for (i = 0; i < N; i++)
    {
        if (shmSM[i].isFree)
        {
            shmSM[i].isFree = 0;
            shmSM[i].pid = getpid();
            logger(LOGFILE, "%s:%d\tFree slot found at index %d", __FILE__, __LINE__, i);
            break;
        }
    }

    /* If no free slot found, return error ENOBUFS */
    if (i == N)
    {
        retval = -1;
        errno = ENOBUFS;
    }

    /* Signal the initmsocket daemon to create a new MTP socket */
    if (sem_post(&shmSOCK_INFO->sem1) == -1)
    {
        perror("sem_post");
        retval = -1;
    }

    /* Wait for the initmsocket daemon to finish creating the new MTP socket */
    if (sem_wait(&shmSOCK_INFO->sem2) == -1)
    {
        perror("sem_wait");
        retval = -1;
    }

    /* Return the file descriptor of the new MTP socket */
    shmSM[i].UDPfd = shmSOCK_INFO->sockfd;
    retval = shmSOCK_INFO->sockfd;

    /* Garbage collection */
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

    if (retval == -1)
    {
        logger(LOGFILE, "%s:%d\tError creating socket: %s", __FILE__, __LINE__, strerror(errno));
    }

    return retval;
}


/*
 *  Function:   m_bind
 *  ------------------
 *  Description:    Bind a MTP socket to a source address and a destination address
 *  Parameters:     sockfd = File descriptor of the MTP socket
 *                  srcaddr = Source address
 *                  srcaddrlen = Length of source address
 *                  destaddr = Destination address
 *                  destaddrlen = Length of destination address
 *  Returns:        int:        0 on success, -1 on error
 */
int
m_bind (int sockfd, const struct sockaddr *srcaddr, socklen_t srcaddrlen, const struct sockaddr *destaddr, socklen_t destaddrlen)
{
    int retval = 0;

    int sizeSM = N * sizeof(struct SM);
    int sizeSOCK_INFO = sizeof(struct SOCK_INFO);

    /* Open shared memory for SM */
    int shmidSM = shm_open(KEY_SM, O_RDWR, 0);
    if (shmidSM == -1)
    {
        perror("shm_open");
        retval = -1;
    }

    /* Map shared memory for SM */
    struct SM *shmSM = mmap(0, sizeSM, PROT_READ | PROT_WRITE, MAP_SHARED, shmidSM, 0);
    if (shmSM == MAP_FAILED)
    {
        perror("mmap");
        retval = -1;
    }

    /* Open shared memory for SOCK_INFO */
    int shmidSOCK_INFO = shm_open(KEY_SOCK_INFO, O_RDWR, 0);
    if (shmidSOCK_INFO == -1)
    {
        perror("shm_open");
        retval = -1;
    }

    /* Map shared memory for SOCK_INFO */
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

    /* Signal the initmsocket daemon to bind the MTP socket */
    if (sem_post(&shmSOCK_INFO->sem1) == -1)
    {
        perror("sem_post");
        retval = -1;
    }

    /* Wait for the initmsocket daemon to finish binding the MTP socket */
    if (sem_wait(&shmSOCK_INFO->sem2) == -1)
    {
        perror("sem_wait");
        retval = -1;
    }

    /* Update the destination address in SM, thus making a pseudo connection */
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

    /* Garbage collection */
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
        logger(LOGFILE, "%s:%d\tError binding socket: %s", __FILE__, __LINE__, strerror(errno));
    }

    return retval;
}


/*
 *  Function:   m_sendto
 *  --------------------
 *  Description:    Send a message to a destination address
 *  Parameters:     sockfd = File descriptor of the MTP socket
 *                  buf = Buffer containing the message to be sent
 *                  len = Length of the message
 *                  flags = Flags
 *                  dest_addr = Destination address
 *                  addrlen = Length of destination address
 *  Returns:        int:        Number of bytes sent on success, -1 on error
 *  Errors:         ENOBUFS:    No buffer space available
 *                  ENOTCONN:   The socket is not connected
 */
int
m_sendto (int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen)
{
    int retval = 0;

    int sizeSM = N * sizeof(struct SM);

    char msg[MAXBUFLEN];
    memset(msg, 0, MAXBUFLEN);

    /* Open shared memory for SM */
    int shmidSM = shm_open(KEY_SM, O_CREAT | O_RDWR, 0666);
    if (shmidSM == -1)
    {
        perror("shm_open");
        retval = -1;
    }

    /* Map shared memory for SM */
    struct SM *shmSM = mmap(0, sizeSM, PROT_READ | PROT_WRITE, MAP_SHARED, shmidSM, 0);
    if (shmSM == MAP_FAILED)
    {
        perror("mmap");
        retval = -1;
    }

    int i;
    /* Find the sockfd in SM */
    for (i = 0; i < N; i++)
    {
        /* If sockfd found, cross-verify the destination address */
        if (shmSM[i].UDPfd == sockfd)
        {
            struct sockaddr *shmSMaddr = (struct sockaddr *)malloc(addrlen);
            *shmSMaddr = *(struct sockaddr *)(&shmSM[i].addr);

            if (memcmp(dest_addr, shmSMaddr, addrlen) == 0)
            {
                /* If destination address matches, put the message in the send buffer */
                if (shmSM[i].sbuff[shmSM[i].lastPut][0] == '\0')
                {
                    char seq[SEQ_LEN + 1];
                    memset(seq, 0, SEQ_LEN + 1);
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
                    memcpy(msg + strlen(MSG) + SEQ_LEN, buf, len);
                    strcat(msg, POSTAMBLE);
                    strcpy(shmSM[i].sbuff[shmSM[i].lastPut], msg);
                    
                    logger(LOGFILE, "%s:%d\tPutting message in send buffer at index %d", __FILE__, __LINE__, shmSM[i].lastPut);
                    
                    shmSM[i].currSeq = (shmSM[i].currSeq + 1) % 16;
                    shmSM[i].lastPut = (shmSM[i].lastPut + 1) % 10;

                    break;
                }
                /* If send buffer is full, return error ENOBUFS */
                else
                {
                    retval = -1;
                    errno = ENOBUFS;
                }
            }
            /* If destination address does not match, return error ENOTCONN */
            else
            {
                retval = -1;
                errno = ENOTCONN;
            }
            break;
        }
    }

    /* If sockfd not found, return error ENOTCONN */
    if (i == N)
    {
        retval = -1;
        errno = ENOTCONN;
    }

    /* Garbage collection */
    if (munmap(shmSM, sizeSM) == -1)
    {
        perror("munmap");
        retval = -1;
    }

    close(shmidSM);

    return retval;
}


/*
 *  Function:   m_recvfrom
 *  ----------------------
 *  Description:    Receive a message from a source address
 *  Parameters:     sockfd = File descriptor of the MTP socket
 *                  buf = Buffer to store the received message
 *                  len = Length of the buffer
 *                  flags = Flags
 *                  src_addr = Source address
 *                  addrlen = Length of source address
 *  Returns:        int:        Number of bytes received on success, -1 on error
 *  Errors:         ENOMSG:     No message available
 *                  ENOTCONN:   The socket is not connected
 */
int
m_recvfrom (int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen)
{
    int retval = 0;

    int sizeSM = N * sizeof(struct SM);

    /* Open shared memory for SM */
    int shmidSM = shm_open(KEY_SM, O_CREAT | O_RDWR, 0666);
    if (shmidSM == -1)
    {
        perror("shm_open");
        retval = errno;
    }

    /* Map shared memory for SM */
    struct SM *shmSM = mmap(0, sizeSM, PROT_READ | PROT_WRITE, MAP_SHARED, shmidSM, 0);
    if (shmSM == MAP_FAILED)
    {
        perror("mmap");
        retval = errno;
    }
    
    int i;
    /* Find the sockfd in SM */
    for (i = 0; i < N; i++)
    {
        if (shmSM[i].UDPfd == sockfd)
        {
            int j;
            /* Find the first non-empty message in the receive buffer */
            for (j = shmSM[i].lastGet; shmSM[i].toConsume != 0; j = (j + 1) % 5)
            {
                if (shmSM[i].rbuff[j][0] != '\0')
                {
                    retval = strlen(shmSM[i].rbuff[j]) - strlen(MSG) - SEQ_LEN - strlen(POSTAMBLE);
                    memcpy(buf, shmSM[i].rbuff[j] + strlen(MSG) + SEQ_LEN, retval);
                    if (strncmp(buf, CLOSE, strlen(CLOSE)) == 0)
                    {
                        return 0;
                    }
                    memset(shmSM[i].rbuff[j], 0, MAXBUFLEN);
                    shmSM[i].lastGet = (j + 1) % 5;
                    shmSM[i].rwnd.size++;
                    shmSM[i].toConsume--;

                    logger(LOGFILE, "%s:%d\tReturning to user %d message from receive buffer at index %d: %s", __FILE__, __LINE__, sockfd, j, buf);

                    break;
                }
            }
            /* If no message found, return error ENOMSG */
            if (j == shmSM[i].lastGet)
            {
                retval = -1;
                errno = ENOMSG;
            }

            break;
        }
    }

    /* If sockfd not found, return error ENOTCONN */
    if (i == N)
    {
        retval = -1;
        errno = ENOTCONN;
    }

    /* Garbage collection */
    if (munmap(shmSM, sizeSM) == -1)
    {
        perror("munmap");
        retval = -1;
    }

    close(shmidSM);

    if (retval < 0)
    {
        // logger(LOGFILE, "%s:%d\tError receiving message: %s", __FILE__, __LINE__, strerror(errno));
    }

    return retval;
}


/*
 *  Function:   m_close
 *  -------------------
 *  Description:    Close a MTP socket
 *  Parameters:     sockfd = File descriptor of the MTP socket
 *  Returns:        int:        0 on success, -1 on error
 *  Errors:         ENOTCONN:   The socket is not connected
 */
int
m_close (int sockfd)
{
    int retval = 0;

    int sizeSM = N * sizeof(struct SM);
    int sizeSOCK_INFO = sizeof(struct SOCK_INFO);

    /* Open shared memory for SM */
    int shmidSM = shm_open(KEY_SM, O_CREAT | O_RDWR, 0666);
    if (shmidSM == -1)
    {
        perror("shm_open");
        retval = errno;
    }

    /* Map shared memory for SM */
    struct SM *shmSM = mmap(0, sizeSM, PROT_READ | PROT_WRITE, MAP_SHARED, shmidSM, 0);
    if (shmSM == MAP_FAILED)
    {
        perror("mmap");
        retval = errno;
    }

    /* Open shared memory for SOCK_INFO */
    int shmidSOCK_INFO = shm_open(KEY_SOCK_INFO, O_CREAT | O_RDWR, 0666);
    if (shmidSOCK_INFO == -1)
    {
        perror("shm_open");
        retval = errno;
    }

    /* Map shared memory for SOCK_INFO */
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
    /* Find the sockfd in SM */
    for (i = 0; i < N; i++)
    {
        /* If sockfd found, mark the slot as free */
        if (shmSM[i].UDPfd == sockfd)
        {
            break;
        }
    }

    /* If sockfd not found, return error ENOTCONN */
    if (i == N)
    {
        retval = -1;
        errno = ENOTCONN;
    }

    /* Signal the initmsocket daemon to close the MTP socket */
    if (sem_post(&shmSOCK_INFO->sem1) == -1)
    {
        perror("sem_post");
        retval = -1;
    }

    /* Wait for the initmsocket daemon to finish closing the MTP socket */
    if (sem_wait(&shmSOCK_INFO->sem2) == -1)
    {
        perror("sem_wait");
        retval = -1;
    }

    errno = shmSOCK_INFO->err;

    /* Garbage collection */
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
        logger(LOGFILE, "%s:%d\tError closing socket: %s", __FILE__, __LINE__, strerror(errno));
    }

    return retval;
}


/*
 *  Function:   dropMessage
 *  ------------------------
 *  Description:    Drop a message with a given probability
 *  Parameters:     p = Probability of dropping the message
 *  Returns:        int:        1 if the message is dropped, 0 otherwise
 */
int
dropMessage(float p)
{
    return (rand() < p * RAND_MAX);
}


/*
 *  Function:   logger
 *  -------------------
 *  Description:    Log a message to a file
 *  Parameters:     fname = File name
 *                  format = Format string
 *                  ... = Arguments
 *  Returns:        int:        0 on success, -1 on error
 */
int
logger (char *fname, const char *format, ...)
{
    /* Open log file, create if not exists */
    FILE *log = fopen(fname, "a");
    if (log == NULL)
    {
        perror("fopen");
        return -1;
    }
    
    /* Log message */
    va_list args;
    va_start(args, format);
    vfprintf(log, format, args);
    va_end(args);
    fprintf(log, "\n");

    /* Close log file */
    fclose(log);

    return 0;
}


/*
 *  Function:   fetchTest
 *  ----------------------
 *  Description:    Fetch the total number of messages and transmissions from the shared memory
 *  Parameters:     p = Probability of dropping the message
 *                  arr = Array to store the total number of messages and transmissions
 *  Returns:        int:        0 on success, -1 on error
 */
int
fetchTest(float p, int *arr)
{
    int sizeSOCK_INFO = sizeof(struct SOCK_INFO);
    int sizeTEST_DATA = sizeof(struct TEST_DATA);

    /* Open shared memory for SOCK_INFO */
    int shmidSOCK_INFO = shm_open(KEY_SOCK_INFO, O_CREAT | O_RDWR, 0666);
    if (shmidSOCK_INFO == -1)
    {
        perror("shm_open");
        return -1;
    }

    /* Map shared memory for SOCK_INFO */
    struct SOCK_INFO *shmSOCK_INFO = mmap(0, sizeSOCK_INFO, PROT_READ | PROT_WRITE, MAP_SHARED, shmidSOCK_INFO, 0);
    if (shmSOCK_INFO == MAP_FAILED)
    {
        perror("mmap");
        return -1;
    }

    /* Open shared memory for TEST_DATA */
    int shmidTEST_DATA = shm_open(KEY_TEST_DATA, O_CREAT | O_RDWR, 0666);
    if (shmidTEST_DATA == -1)
    {
        perror("shm_open");
        return -1;
    }

    /* Map shared memory for TEST_DATA */
    struct TEST_DATA *shmTEST_DATA = mmap(0, sizeTEST_DATA, PROT_READ | PROT_WRITE, MAP_SHARED, shmidTEST_DATA, 0);
    if (shmTEST_DATA == MAP_FAILED)
    {
        perror("mmap");
        return -1;
    }

    shmTEST_DATA->p = p;

    shmSOCK_INFO->sockfd = -1;
    shmSOCK_INFO->err = -1;


    /* Signal the initmsocket daemon to bind the MTP socket */
    if (sem_post(&shmSOCK_INFO->sem1) == -1)
    {
        perror("sem_post");
        return -1;
    }

    /* Wait for the initmsocket daemon to finish binding the MTP socket */
    if (sem_wait(&shmSOCK_INFO->sem2) == -1)
    {
        perror("sem_wait");
        return -1;
    }

    arr[0] = shmTEST_DATA->totalMessages;
    arr[1] = shmTEST_DATA->totalTransmissions;

    /* Garbage collection */
    if (munmap(shmSOCK_INFO, sizeSOCK_INFO) == -1)
    {
        perror("munmap");
        return -1;
    }

    if (munmap(shmTEST_DATA, sizeTEST_DATA) == -1)
    {
        perror("munmap");
        return -1;
    }

    close(shmidSOCK_INFO);
    close(shmidTEST_DATA);

    return 0;
}