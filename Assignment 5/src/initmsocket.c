/*
 *  Author:     Prasanna Paithankar
 *  Roll No.:   21CS30065
 *  Course:     Computer Networks Laboratory (CS39006) Spr 2023-24
 *  Date:       15/03/2024

 *  File:       initmsocket.c
 *  Compile:    gcc -o initmsocket initmsocket.c -L. -lmsocket -lpthread -lrt
 *  Run:        ./initmsocket
 */

#include "msocket.h"

/* Benchmarking parameters */
int totalMessages = 0;
int totalTransmissions = 0;

/* Semaphore for closing the socket */
sem_t initSem;

/* Function prototypes */
void *
Rthread(void *arg);

void *
Sthread(void *arg);

void *
Gthread(void *arg);


/*
 *  Function:   main
 *  -------------
 *  Description:    Initializes the shared memory and semaphores, creates the R, S and G threads,
 *                  and handles the socket creation, binding and closing requests
 */
int
main(int argc, char *argv[])
{
    pthread_t R, S, G;

    int sizeSM = N * sizeof(struct SM);
    int sizeSOCK_INFO = sizeof(struct SOCK_INFO);

    /* Initialize shared memory SM */
    int shmidSM = shm_open(KEY_SM, O_CREAT | O_RDWR, 0666);
    if (shmidSM == -1)
    {
        perror("shm_open");
        exit(1);
    }

    /* Truncate the shared memory to the required size */
    if (ftruncate(shmidSM, sizeSM) == -1)
    {
        perror("ftruncate");
        exit(1);
    }

    /* Map the shared memory to the process's address space */
    struct SM *shmSM = mmap(0, sizeSM, PROT_READ | PROT_WRITE, MAP_SHARED, shmidSM, 0);
    if (shmSM == MAP_FAILED)
    {
        perror("mmap");
        exit(1);
    }

    /* Initialize the shared memory */
    for (int i = 0; i < N; i++)
    {
        shmSM[i].isFree = 1;
        shmSM[i].pid = 0;
        shmSM[i].UDPfd = 0;
        memset(&shmSM[i].addr, 0, sizeof(shmSM[i].addr));
        memset(shmSM[i].sbuff, 0, sizeof(shmSM[i].sbuff));
        memset(shmSM[i].rbuff, 0, sizeof(shmSM[i].rbuff));
        memset(&shmSM[i].swnd, 0, sizeof(shmSM[i].swnd));
        memset(&shmSM[i].rwnd, 0, sizeof(shmSM[i].rwnd));
        shmSM[i].swnd.size = 5;
        shmSM[i].rwnd.size = 5;
        shmSM[i].swnd.base = 0;
        shmSM[i].rwnd.base = 0;
        shmSM[i].currSeq = 0;
        shmSM[i].currExpSeq = 0;
        shmSM[i].lastAck = 15;
        shmSM[i].lastPut = 0;
        shmSM[i].lastGet = 0;
        shmSM[i].toConsume = 0;
    }
    printf("SM initialized\n");
    logger(LOGFILE, "SM initialized");

    /* Initialize shared memory SOCK_INFO */
    int shmidSOCK_INFO = shm_open(KEY_SOCK_INFO, O_CREAT | O_RDWR, 0666);
    if (shmidSOCK_INFO == -1)
    {
        perror("shm_open");
        exit(1);
    }

    /* Truncate the shared memory to the required size */
    if (ftruncate(shmidSOCK_INFO, sizeSOCK_INFO) == -1)
    {
        perror("ftruncate");
        exit(1);
    }

    /* Map the shared memory to the process's address space */
    struct SOCK_INFO *shmSOCK_INFO = mmap(0, sizeSOCK_INFO, PROT_READ | PROT_WRITE, MAP_SHARED, shmidSOCK_INFO, 0);
    if (shmSOCK_INFO == MAP_FAILED)
    {
        perror("mmap");
        exit(1);
    }
    memset(shmSOCK_INFO, 0, sizeSOCK_INFO);

    /* Initialize the semaphores */
    if (sem_init(&shmSOCK_INFO->sem1, 1, 0) == -1)
    {
        perror("sem_init");
        exit(1);
    }

    if (sem_init(&shmSOCK_INFO->sem2, 1, 0) == -1)
    {
        perror("sem_init");
        exit(1);
    }
    printf("SOCK_INFO initialized\n");
    logger(LOGFILE, "SOCK_INFO initialized");

    if (sem_init(&initSem, 1, 0) == -1)
    {
        perror("sem_init");
        exit(1);
    }

    /* Create the R, S and G threads */
    if (pthread_create(&R, NULL, Rthread, (void *)shmSM) != 0)
    {
        perror("pthread_create");
        exit(1);
    }
    printf("Rthread created\n");
    logger(LOGFILE, "%s:%d\tRthread created", __FILE__, __LINE__);

    if (pthread_create(&S, NULL, Sthread, (void *)shmSM) != 0)
    {
        perror("pthread_create");
        exit(1);
    }
    printf("Sthread created\n");
    logger(LOGFILE, "%s:%d\tSthread created", __FILE__, __LINE__);

    if (pthread_create(&G, NULL, Gthread, (void *)shmSM) != 0)
    {
        perror("pthread_create");
        exit(1);
    }
    printf("Gthread created\n");
    logger(LOGFILE, "%s:%d\tGthread created", __FILE__, __LINE__);

    /* Handle the socket creation, binding and closing requests */
    while (1)
    {
        /* Wait for the request */
        if (sem_wait(&(shmSOCK_INFO->sem1)) == -1)
        {
            perror("sem_wait");
            continue;
        }

        char srcIP[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(shmSOCK_INFO->addr.sin_addr), srcIP, INET_ADDRSTRLEN);

        /* Handle create socket request */
        if (shmSOCK_INFO->sockfd == 0 && shmSOCK_INFO->addr.sin_port == 0 && shmSOCK_INFO->err == 0)
        {
            logger(LOGFILE, "%s:%d\tCreating socket", __FILE__, __LINE__);
            shmSOCK_INFO->sockfd = socket(AF_INET, SOCK_DGRAM, 0);
            if (shmSOCK_INFO->sockfd == -1)
            {
                perror("socket");
                shmSOCK_INFO->err = errno;
            }
            else
            {
                shmSOCK_INFO->err = 0;
            }
        }

        /* Handle bind socket request */
        else if (shmSOCK_INFO->sockfd != 0 && shmSOCK_INFO->addr.sin_port != 0)
        {
            logger(LOGFILE, "%s:%d\tBinding socket", __FILE__, __LINE__);
            if (bind(shmSOCK_INFO->sockfd, (struct sockaddr *)&(shmSOCK_INFO->addr), sizeof(shmSOCK_INFO->addr)) == -1)
            {
                perror("bind");
                shmSOCK_INFO->err = errno;
            }
            else
            {
                shmSOCK_INFO->err = 0;
            }
        }

        /* Handle close socket request */
        else if (shmSOCK_INFO->sockfd != 0 && shmSOCK_INFO->addr.sin_port == 0 && shmSOCK_INFO->addr.sin_addr.s_addr == 0)
        {
            int i;
            for (i = 0; i < N; i++)
            {
                if (shmSM[i].isFree == 0)
                {
                    if (shmSM[i].UDPfd == shmSOCK_INFO->sockfd)
                    {
                        break;
                    }
                }
            }

            struct timespec ts;
            if (clock_gettime(CLOCK_REALTIME, &ts) == -1)
            {
                shmSOCK_INFO->err = errno;
            }
            ts.tv_sec += (T/2);

            int count = 0;
            /* Send CLOSE message at most MAXCLOSECALLS times */
            char closemsg[MAXBUFLEN];
            while (1)
            {
                if (count == MAXCLOSECALLS)
                {
                    break;
                }
                count++;
                memset(closemsg, 0, sizeof(closemsg));
                strcpy(closemsg, CLOSE);
                strcat(closemsg, POSTAMBLE);
                if (sendto(shmSOCK_INFO->sockfd, closemsg, strlen(closemsg), 0, (struct sockaddr *)&(shmSM[i].addr), sizeof(shmSM[i].addr)) == -1)
                {
                    shmSOCK_INFO->err = errno;
                }
                if (sem_timedwait(&initSem, &ts) == -1)
                {
                    shmSOCK_INFO->err = errno;
                }
                else
                {
                    shmSOCK_INFO->err = 0;
                    break;
                }
            }
            logger(LOGFILE, "%s:%d\tClosing socket", __FILE__, __LINE__);

            /* Close the socket */
            if (close(shmSM[i].UDPfd) == -1)
            {
                shmSOCK_INFO->err = errno;
            }
            
            shmSM[i].isFree = 1;
            shmSM[i].pid = 0;
            memset(&shmSM[i].addr, 0, sizeof(shmSM[i].addr));
            memset(shmSM[i].sbuff, 0, sizeof(shmSM[i].sbuff));
            memset(shmSM[i].rbuff, 0, sizeof(shmSM[i].rbuff));
            memset(&shmSM[i].swnd, 0, sizeof(shmSM[i].swnd));
            memset(&shmSM[i].rwnd, 0, sizeof(shmSM[i].rwnd));
            shmSM[i].swnd.size = 5;
            shmSM[i].rwnd.size = 5;
            shmSM[i].swnd.base = 0;
            shmSM[i].rwnd.base = 0;
            shmSM[i].currSeq = 0;
            shmSM[i].currExpSeq = 0;
            shmSM[i].lastAck = 15;
            shmSM[i].lastPut = 0;
            shmSM[i].lastGet = 0;
            shmSM[i].toConsume = 0;
        }

        else
        {
            logger(LOGFILE, "%s:%d\tInvalid request", __FILE__, __LINE__);
            shmSOCK_INFO->err = EINVAL;
        }

        if (shmSOCK_INFO->err != 0)
        {
            logger(LOGFILE, "%s:%d\t%s", __FILE__, __LINE__, strerror(shmSOCK_INFO->err));
        }

        /* Signal the completion of the request */
        if (sem_post(&(shmSOCK_INFO->sem2)) == -1)
        {
            perror("sem_post");
            continue;
        }
    }

    /* Join the R, S and G threads */
    if (pthread_join(R, NULL) != 0)
    {
        perror("pthread_join");
        exit(1);
    }
    logger(LOGFILE, "%s:%d\tRthread joined", __FILE__, __LINE__);

    if (pthread_join(S, NULL) != 0)
    {
        perror("pthread_join");
        exit(1);
    }
    logger(LOGFILE, "%s:%d\tSthread joined", __FILE__, __LINE__);

    if (pthread_join(G, NULL) != 0)
    {
        perror("pthread_join");
        exit(1);
    }
    logger(LOGFILE, "%s:%d\tGthread joined", __FILE__, __LINE__);

    /* Garbage collection */
    if (sem_destroy(&shmSOCK_INFO->sem1) == -1)
    {
        perror("sem_destroy");
        exit(1);
    }

    if (sem_destroy(&shmSOCK_INFO->sem2) == -1)
    {
        perror("sem_destroy");
        exit(1);
    }

    if (munmap(shmSM, sizeSM) == -1)
    {
        perror("munmap");
        exit(1);
    }

    if (munmap(shmSOCK_INFO, sizeSOCK_INFO) == -1)
    {
        perror("munmap");
        exit(1);
    }

    if (shm_unlink(KEY_SM) == -1)
    {
        perror("shm_unlink");
        exit(1);
    }

    if (shm_unlink(KEY_SOCK_INFO) == -1)
    {
        perror("shm_unlink");
        exit(1);
    }

    return 0;
}


/*
 *  Function:   Rthread
 *  -------------------
 *  Description:    Function to handle the reception of messages (to be run as a thread)
 */
void *
Rthread(void *arg)
{
    int err = 0;
    int nospace[N] = {0};
    struct SM *shm = (struct SM *)arg;

    struct epoll_event ev, events[25];
    int nfds, epollfd;

    char closeackmsg[MAXBUFLEN];
    memset(closeackmsg, 0, sizeof(closeackmsg));
    strcpy(closeackmsg, CLOSE);
    strcat(closeackmsg, ACK);
    strcat(closeackmsg, POSTAMBLE);

    /* Create the epoll file descriptor */
    epollfd = epoll_create(25);
    if (epollfd == -1)
    {
        perror("epoll_create");
    }

    int i;
    for (i = 0; i < N; i++)
    {
        if (shm[i].isFree == 0)
        {
            ev.events = EPOLLIN;
            ev.data.fd = shm[i].UDPfd;
            if (epoll_ctl(epollfd, EPOLL_CTL_ADD, shm[i].UDPfd, &ev) == -1)
            {
                perror("epoll_ctl");
                err = errno;
            }
        }
    }

    while (1)
    {
        /* Wait for the events */
        nfds = epoll_wait(epollfd, events, 25, T * 1000);
        if (nfds == -1)
        {
            perror("epoll_wait");
            continue;
        }

        else if (nfds > 0)
        {
            for (i = 0; i < nfds; i++)
            {
                if (events[i].events & EPOLLIN)
                {
                    int j;
                    for (j = 0; j < N; j++)
                    {
                        if (shm[j].UDPfd == events[i].data.fd)
                        {
                            break;
                        }
                    }

                    if (j == N)
                    {
                        logger(LOGFILE, "%s:%d\tInvalid socket", __FILE__, __LINE__);
                        continue;
                    }

                    struct sockaddr_in src_addr;
                    socklen_t addrlen = sizeof(src_addr);
                    char buf[MAXBUFLEN];
                    char msg[MAXBUFLEN];
                    memset(buf, 0, MAXBUFLEN);
                    memset(msg, 0, MAXBUFLEN);

                    /* Receive the message */
                    while (1)
                    {
                        int numbytes = recvfrom(shm[j].UDPfd, buf, MAXBUFLEN - 1, 0, (struct sockaddr *)&src_addr, &addrlen);
                        if (numbytes == -1)
                        {
                            perror("recvfrom");
                            break;
                        }
                        strcat(msg, buf);
                        memset(buf, 0, MAXBUFLEN);
                        if (strstr(msg, POSTAMBLE) != NULL)
                        {
                            break;
                        }
                    }

                    /* Drop the message with probability P */
                    if (dropMessage(P) == 0)
                    {
                        /* Handle the MSG message */
                        if (memcmp(msg, MSG, strlen(MSG)) == 0)
                        {
                            logger(LOGFILE, "%s:%d\tMessage %s received from %s:%d", __FILE__, __LINE__, msg, inet_ntoa(src_addr.sin_addr), ntohs(src_addr.sin_port));
                            
                            char data[MAXBUFLEN];
                            char seqno[SEQ_LEN + 1];
                            memset(data, 0, sizeof(data));
                            memset(seqno, 0, sizeof(seqno));
                            memcpy(data, msg + strlen(MSG) + SEQ_LEN, strlen(msg) - strlen(MSG) - strlen(POSTAMBLE) - SEQ_LEN);
                            memcpy(seqno, msg + strlen(MSG), SEQ_LEN);

                            int k;
                            int pos = 0;

                            int count = 0;
                            for (k = shm[j].currExpSeq; ; k = (k + 1) % 16)
                            {
                                if (count == shm[j].rwnd.size)
                                {
                                    break;
                                }
                                count++;
                                if (k == atoi(seqno))
                                {
                                    break;
                                }
                                pos++;
                            }

                            if (k == (shm[j].currExpSeq + shm[j].rwnd.size) % 16)
                            {
                                logger(LOGFILE, "%s:%d\tDuplicate message", __FILE__, __LINE__);
                            }

                            else
                            {
                                logger(LOGFILE, "%s:%d\tMessage is put in receive buffer at position: %d", __FILE__, __LINE__, shm[j].rwnd.base + pos);
                                memcpy(shm[j].rbuff[shm[j].rwnd.base + pos], msg, strlen(msg));
                                if (pos == 0)
                                {
                                    int count = 0;
                                    int lastposack = 0;
                                    for (int l = shm[j].rwnd.base; ; l = (l + 1) % 5)
                                    {
                                        if (count == shm[j].rwnd.size)
                                        {
                                            break;
                                        }
                                        count++;
                                        if (shm[j].rbuff[l][0] != '\0')
                                        {
                                            shm[j].lastAck = (shm[j].lastAck + 1) % 16;
                                            lastposack++;
                                        }
                                        else
                                        {
                                            break;
                                        }
                                    }
                                    shm[j].rwnd.base = (shm[j].rwnd.base + lastposack) % 5;
                                    shm[j].rwnd.size -= lastposack;
                                    shm[j].toConsume += lastposack;
                                    shm[j].currExpSeq = (shm[j].currExpSeq + lastposack) % 16;
                                    if (shm[j].rwnd.size == 0)
                                    {
                                        nospace[j] = 1;
                                    }
                                }
                                else
                                {
                                    logger(LOGFILE, "%s:%d\tMessage recieved out of order", __FILE__, __LINE__);
                                }
                            }

                            /* Send the ACK message */
                            char ack[MAXBUFLEN];
                            memset(ack, 0, sizeof(ack));
                            strcpy(ack, ACK);
                            char sseqno[SEQ_LEN + 1];
                            if (shm[j].lastAck < 10)
                            {
                                sprintf(sseqno, "0%d", shm[j].lastAck);
                            }
                            else
                            {
                                sprintf(sseqno, "%d", shm[j].lastAck);
                            }
                            strcat(ack, sseqno);
                            char ssize[SEQ_LEN + 1];
                            sprintf(ssize, "%d", shm[j].rwnd.size);
                            strcat(ack, ssize);
                            strcat(ack, POSTAMBLE);
                            if (sendto(shm[j].UDPfd, ack, strlen(ack), 0, (struct sockaddr *)&src_addr, addrlen) == -1)
                            {
                                perror("sendto");
                            }

                            logger(LOGFILE, "%s:%d\tACK sent for sequence number: %d", __FILE__, __LINE__, shm[j].lastAck);
                        }
                        
                        /* Handle the ACK messages */
                        else if (memcmp(msg, ACK, strlen(ACK)) == 0)
                        {
                            logger(LOGFILE, "%s:%d\tACK %s received from %s:%d", __FILE__, __LINE__, msg, inet_ntoa(src_addr.sin_addr), ntohs(src_addr.sin_port));
                            char data[MAXBUFLEN];
                            char seqno[SEQ_LEN + 1];
                            memset(data, 0, sizeof(data));
                            memset(seqno, 0, sizeof(seqno));
                            memcpy(data, msg + strlen(ACK) + SEQ_LEN, strlen(msg) - strlen(ACK) - strlen(POSTAMBLE) - SEQ_LEN);
                            memcpy(seqno, msg + strlen(ACK), SEQ_LEN);

                            int k, count = 0;
                            for (k = shm[j].swnd.base; ; k = (k + 1) % 10)
                            {
                                if (count == shm[j].swnd.size)
                                {
                                    break;
                                }
                                count++;
                                char sseqno[SEQ_LEN + 1];
                                memset(sseqno, 0, sizeof(sseqno));
                                memcpy(sseqno, shm[j].sbuff[k] + strlen(MSG), SEQ_LEN);
                                if (memcmp(sseqno, seqno, SEQ_LEN) == 0)
                                {
                                    break;
                                }
                            }

                            if (k == (shm[j].swnd.base + shm[j].swnd.size) % 10)
                            {
                                logger(LOGFILE, "%s:%d\tDuplicate ACK", __FILE__, __LINE__);
                            }
                            else
                            {
                                int cnt = 0;
                                for (int l = shm[j].swnd.base; ; l = (l + 1) % 10)
                                {
                                    if (cnt == count)
                                    {
                                        break;
                                    }
                                    cnt++;
                                    memset(shm[j].sbuff[l], 0, sizeof(shm[j].sbuff[l]));
                                    shm[j].swnd.timestamp[l] = 0;
                                }
                                shm[j].swnd.base = (k + 1) % 10;
                            }
                            shm[j].swnd.size = atoi(data);
                        }

                        /* Handle CLOSEACK message */

                        else if (memcmp(msg, closeackmsg, strlen(closeackmsg)) == 0)
                        {
                            logger(LOGFILE, "%s:%d\tCLOSEACK received", __FILE__, __LINE__);
                            if (sem_post(&initSem) == -1)
                            {
                                perror("sem_post");
                            }
                        }

                        /* Handle CLOSE message */
                        else if (memcmp(msg, CLOSE, strlen(CLOSE)) == 0)
                        {
                            logger(LOGFILE, "%s:%d\tClosing message received", __FILE__, __LINE__);
                            
                            /* Send the CLOSEACK message */
                            if (sendto(shm[j].UDPfd, closeackmsg, strlen(closeackmsg), 0, (struct sockaddr *)&shm[j].addr, sizeof(shm[j].addr)) == -1)
                            {
                                perror("sendto");
                            }
                            logger(LOGFILE, "%s:%d\tCLOSEACK sent", __FILE__, __LINE__);

                            /* Put in rbuff */
                            memcpy(shm[j].rbuff[shm[j].rwnd.base], msg, strlen(msg));
                            shm[j].rwnd.size--;
                            shm[j].rwnd.base = (shm[j].rwnd.base + 1) % 5;
                            shm[j].toConsume++;
                            logger(LOGFILE, "%s:%d\tClose message put in receive buffer at position: %d", __FILE__, __LINE__, shm[j].rwnd.base);
                        }

                        else
                        {
                            logger(LOGFILE, "%s:%d\tInvalid message type", __FILE__, __LINE__);
                        }
                    }

                    else
                    {
                        printf("Message dropped\n");
                        logger(LOGFILE, "%s:%d\tMessage dropped", __FILE__, __LINE__);
                    }

                    if (err != 0)
                    {
                        // logger(LOGFILE, "%s:%d\t%s", __FILE__, __LINE__, strerror(err));
                    }
                }

                else
                {
                    logger(LOGFILE, "%s:%d\tInvalid event", __FILE__, __LINE__);
                }

                if (err != 0)
                {
                    // logger(LOGFILE, "%s:%d\t%s", __FILE__, __LINE__, strerror(err));
                }
            }
        }

        /* Event timeout */
        else
        {
            for (i = 0; i < N; i++)
            {
                /* If nospace is set and receive window is not empty, send the ACK message */
                if (nospace[i] == 1)
                {
                    logger(LOGFILE, "%s:%d\tNo space in receive window", __FILE__, __LINE__);
                    if (shm[i].rwnd.size != 0)
                    {
                        char ack[MAXBUFLEN];
                        memset(ack, 0, sizeof(ack));
                        strcpy(ack, ACK);
                        char sseqno[SEQ_LEN + 1];
                        if (shm[i].lastAck < 10)
                        {
                            sprintf(sseqno, "0%d", shm[i].lastAck);
                        }
                        else
                        {
                            sprintf(sseqno, "%d", shm[i].lastAck);
                        }
                        strcat(ack, sseqno);
                        char ssize[SEQ_LEN + 1];
                        sprintf(ssize, "%d", shm[i].rwnd.size);
                        strcat(ack, ssize);
                        strcat(ack, POSTAMBLE);
                        if (sendto(shm[i].UDPfd, ack, strlen(ack), 0, (struct sockaddr *)&(shm[i].addr), sizeof(shm[i].addr)) == -1)
                        {
                            perror("sendto");
                        }
                        logger(LOGFILE, "%s:%d\tACK sent for sequence number: %d", __FILE__, __LINE__, shm[i].lastAck);

                        nospace[i] = 0;
                    }
                }
            }

            /* Re-register the sockets */
            for (i = 0; i < N; i++)
            {
                if (shm[i].isFree == 0)
                {
                    ev.events = EPOLLIN;
                    ev.data.fd = shm[i].UDPfd;
                    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, shm[i].UDPfd, &ev) == -1)
                    {
                        // perror("epoll_ctl");
                        err = errno;
                    }
                }
            }

            if (err != 0)
            {
                // logger(LOGFILE, strerror(err));
            }
        }
    }

    return NULL;
}


/*
 *  Function:   Sthread
 *  -------------------
 *  Description:    Function to handle the transmission of messages (to be run as a thread)
 */
void *
Sthread(void *arg)
{
    struct SM *shm = (struct SM *)arg;
    int i;

    while (1)
    {
        for (i = 0; i < N; i++)
        {
            if (shm[i].isFree == 0)
            {
                int timedout = 0;
                int count = 0;

                /* Check for timeout */
                for (int j = shm[i].swnd.base; ; j = (j + 1) % 10)
                {
                    if (count == shm[i].swnd.size)
                    {
                        break;
                    }
                    count++;

                    /* If the message has not been acknowledged and the timeout has occurred, resend all the messages in the send window */
                    if (shm[i].swnd.timestamp[j] != 0 && (time(NULL) - shm[i].swnd.timestamp[j]) > T)
                    {
                        int cnt = 0;
                        for (int k = shm[i].swnd.base; ; k++)
                        {
                            if (cnt == shm[i].swnd.size)
                            {
                                break;
                            }
                            cnt++;
                            shm[i].swnd.timestamp[k] = time(NULL);
                            if (sendto(shm[i].UDPfd, shm[i].sbuff[k], strlen(shm[i].sbuff[k]), 0, (struct sockaddr *)&(shm[i].addr), sizeof(shm[i].addr)) == -1)
                            {
                                perror("sendto");
                            }
                        }
                        timedout = 1;
                        logger(LOGFILE, "%s:%d\tTimeout: Resending %d messages", __FILE__, __LINE__, shm[i].swnd.size);
                        break;
                    }
                }

                /* If no timeout has occurred, send the messages in the send window */
                if (timedout == 0)
                {
                    count = 0;
                    for (int j = shm[i].swnd.base; ; j = (j + 1) % 10)
                    {
                        if (count == shm[i].swnd.size)
                        {
                            break;
                        }
                        count++;
                        if (shm[i].swnd.timestamp[j] == 0 && shm[i].sbuff[j][0] != '\0')
                        {
                            shm[i].swnd.timestamp[j] = time(NULL);

                            if (sendto(shm[i].UDPfd, shm[i].sbuff[j], strlen(shm[i].sbuff[j]), 0, (struct sockaddr *)&(shm[i].addr), sizeof(shm[i].addr)) == -1)
                            {
                                perror("sendto");
                            }
                            char sseqno[SEQ_LEN + 1];
                            memset(sseqno, 0, sizeof(sseqno));
                            memcpy(sseqno, shm[i].sbuff[j] + strlen(MSG), SEQ_LEN);
                            logger(LOGFILE, "%s:%d\tMessage sent with sequence number: %d", __FILE__, __LINE__, atoi(sseqno));
                        }
                    }
                }
            }
        }

        sleep(T / 2);
    }

    return NULL;
}


/*
 *  Function:   Gthread
 *  -------------------
 *  Description:    Function to garbage collect the sockets (to be run as a thread)
 */
void *
Gthread(void *arg)
{
    struct SM *shm = (struct SM *)arg;
    int i;

    while (1)
    {
        for (i = 0; i < N; i++)
        {
            if (shm[i].isFree == 0)
            {
                /* Check if the process that created the socket is still alive */
                if (kill(shm[i].pid, 0) == -1)
                {
                    /* If the process is not alive, close the socket */
                    if (errno == ESRCH)
                    {
                        m_close(shm[i].UDPfd);
                        logger(LOGFILE, "%s:%d\tSocket cleaned", __FILE__, __LINE__);
                    }
                }
            }
        }
        
        sleep(T);
    }

    return NULL;
}
