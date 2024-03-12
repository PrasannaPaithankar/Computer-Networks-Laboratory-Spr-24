#include "msocket.h"

void *Rthread (void *arg);

void *Sthread (void *arg);

void *Gthread (void *arg);


int
main (int argc, char *argv[])
{
    pthread_t R, S, G;

    int sizeSM = N * sizeof(struct SM);
    int sizeSOCK_INFO = sizeof(struct SOCK_INFO);

    int shmidSM = shm_open(KEY_SM, O_CREAT | O_RDWR, 0666);
    if (shmidSM == -1)
    {
        perror("shm_open");
        exit(1);
    }

    if (ftruncate(shmidSM, sizeSM) == -1)
    {
        perror("ftruncate");
        exit(1);
    }

    struct SM *shmSM = mmap(0, sizeSM, PROT_READ | PROT_WRITE, MAP_SHARED, shmidSM, 0);
    if (shmSM == MAP_FAILED)
    {
        perror("mmap");
        exit(1);
    }

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
        shmSM[i].swnd.size = 10;
        shmSM[i].rwnd.size = 5;
    }

    int shmidSOCK_INFO = shm_open(KEY_SOCK_INFO, O_CREAT | O_RDWR, 0666);
    if (shmidSOCK_INFO == -1)
    {
        perror("shm_open");
        exit(1);
    }

    if (ftruncate(shmidSOCK_INFO, sizeSOCK_INFO) == -1)
    {
        perror("ftruncate");
        exit(1);
    }

    struct SOCK_INFO *shmSOCK_INFO = mmap(0, sizeSOCK_INFO, PROT_READ | PROT_WRITE, MAP_SHARED, shmidSOCK_INFO, 0);
    if (shmSOCK_INFO == MAP_FAILED)
    {
        perror("mmap");
        exit(1);
    }
    memset(shmSOCK_INFO, 0, sizeSOCK_INFO);

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

    if (pthread_create(&R, NULL, Rthread, (void *)shmSM) != 0)
    {
        perror("pthread_create");
        exit(1);
    }
    logger(LOGFILE, "Rthread created");

    if (pthread_create(&S, NULL, Sthread, (void *)shmSM) != 0)
    {
        perror("pthread_create");
        exit(1);
    }
    logger(LOGFILE, "Sthread created");

    if (pthread_create(&G, NULL, Gthread, (void *)shmSM) != 0)
    {
        perror("pthread_create");
        exit(1);
    }
    logger(LOGFILE, "Gthread created");

    while (1)
    {
        if (sem_wait(&(shmSOCK_INFO->sem1)) == -1)
        {
            perror("sem_wait");
            continue;
        }

        if (shmSOCK_INFO->sockfd == 0 && shmSOCK_INFO->addr.sin_port == 0 && shmSOCK_INFO->err == 0)
        {
            logger(LOGFILE, "Creating socket");
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

        else if (shmSOCK_INFO->sockfd != 0 && shmSOCK_INFO->addr.sin_port != 0 && shmSOCK_INFO->addr.sin_addr.s_addr != 0)
        {
            logger(LOGFILE, "Binding socket");
            if (bind(shmSOCK_INFO->sockfd, (struct sockaddr *)&shmSOCK_INFO->addr, sizeof(shmSOCK_INFO->addr)) == -1)
            {
                perror("bind");
                shmSOCK_INFO->err = errno;
            }
            else
            {
                shmSOCK_INFO->err = 0;
            }
        }

        else if (shmSOCK_INFO->sockfd != 0 && shmSOCK_INFO->addr.sin_port == 0 && shmSOCK_INFO->addr.sin_addr.s_addr == 0)
        {
            logger(LOGFILE, "Closing socket");
            if (close(shmSOCK_INFO->sockfd) == -1)
            {
                perror("close");
                shmSOCK_INFO->err = errno;
            }
            else
            {
                shmSOCK_INFO->err = 0;
            }
        }

        else
        {
            logger(LOGFILE, "Invalid request");
            shmSOCK_INFO->err = EINVAL;
        }

        if (shmSOCK_INFO->err != 0)
        {
            logger(LOGFILE, strerror(shmSOCK_INFO->err));
        }

        if (sem_post(&(shmSOCK_INFO->sem2)) == -1)
        {
            perror("sem_post");
            continue;
        }
    }

    if (pthread_join(R, NULL) != 0)
    {
        perror("pthread_join");
        exit(1);
    }
    logger(LOGFILE, "Rthread joined");

    if (pthread_join(S, NULL) != 0)
    {
        perror("pthread_join");
        exit(1);
    }
    logger(LOGFILE, "Sthread joined");

    if (pthread_join(G, NULL) != 0)
    {
        perror("pthread_join");
        exit(1);
    }
    logger(LOGFILE, "Gthread joined");

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

void *
Rthread (void *arg)
{
    int err = 0;
    int nospace = 0;
    struct SM *shm = (struct SM *)arg;

    struct epoll_event ev, events[25];
    int nfds, epollfd;

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
        nfds = epoll_wait(epollfd, events, 25, T*1000);
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
                        logger(LOGFILE, "Invalid socket");
                        continue;
                    }

                    struct sockaddr_in src_addr;
                    socklen_t addrlen = sizeof(src_addr);
                    char buf[MAXBUFLEN];
                    char msg[MAXBUFLEN];
                    memset(buf, 0, MAXBUFLEN);
                    memset(msg, 0, MAXBUFLEN);

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

                    if (memcmp(msg, MSG, strlen(MSG)) == 0)
                    {
                        int k;
                        for (k = 0; k < 5; k++)
                        {
                            if (shm[j].rbuff[k][0] == '\0')
                            {
                                strcpy(shm[j].rbuff[k], msg);
                                break;
                            }
                        }
                        if (k == 10)
                        {
                            nospace = 1;
                            logger(LOGFILE, "Receive buffer full");
                        }
                    }

                    else if (memcmp(msg, ACK, strlen(ACK)) == 0)
                    {
                        int k;
                        for (k = 0; k < 5; k++)
                        {
                            if (memcmp(shm[j].sbuff[k], msg + strlen(ACK), strlen(msg) - strlen(ACK) - strlen(POSTAMBLE)) == 0)
                            {
                                memset(shm[j].sbuff[k], 0, sizeof(shm[j].sbuff[k]));
                                break;
                            }
                        }
                        if (k == 5)
                        {
                            int l;
                            for (l = 0; l < 5; l++)
                            {
                                if (memcmp(shm[j].sbuff[l], msg + strlen(ACK), strlen(msg) - strlen(ACK) - strlen(POSTAMBLE)) == 0)
                                {
                                    break;
                                }
                            }
                            if (l == 5)
                            {
                                logger(LOGFILE, "Duplicate ACK");
                            }
                        }
                    }

                    else
                    {
                        logger(LOGFILE, "Invalid message");
                    }
                }

                else
                {
                    logger(LOGFILE, "Invalid event");
                }

                if (err != 0)
                {
                    logger(LOGFILE, strerror(err));
                }
            }
        }

        else
        {
            if (nospace == 1)
            {
                nospace = 0;
            }

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

            if (err != 0)
            {
                logger(LOGFILE, strerror(err));
            }
        }
    }

    return NULL;
}

void *
Sthread (void *arg)
{
    return NULL;
}

void *
Gthread (void *arg)
{
    struct SM *shm = (struct SM *)arg;
    int i;

    while (1)
    {
        for (i = 0; i < N; i++)
        {
            if (shm[i].isFree == 0)
            {
                if (kill(shm[i].pid, 0) == -1)
                {
                    if (errno == ESRCH)
                    {
                        shm[i].isFree = 1;
                        shm[i].pid = 0;
                        close(shm[i].UDPfd);
                        memset(&shm[i].addr, 0, sizeof(shm[i].addr));
                        memset(shm[i].sbuff, 0, sizeof(shm[i].sbuff));
                        memset(shm[i].rbuff, 0, sizeof(shm[i].rbuff));
                        memset(&shm[i].swnd, 0, sizeof(shm[i].swnd));
                        memset(&shm[i].rwnd, 0, sizeof(shm[i].rwnd));
                        shm[i].swnd.size = 10;
                        shm[i].rwnd.size = 5;

                        logger(LOGFILE, "Socket cleaned");
                    }
                }
            }
        }
        sleep(T);
    }

    return NULL;
}
