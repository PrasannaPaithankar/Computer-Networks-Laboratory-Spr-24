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

    if (pthread_create(&S, NULL, Sthread, (void *)shmSM) != 0)
    {
        perror("pthread_create");
        exit(1);
    }

    if (pthread_create(&G, NULL, Gthread, (void *)shmSOCK_INFO) != 0)
    {
        perror("pthread_create");
        exit(1);
    }

    while (1)
    {
        if (sem_wait(&shmSOCK_INFO->sem1) == -1)
        {
            perror("sem_wait");
            exit(1);
        }

        if (shmSOCK_INFO->sockfd == 0 && shmSOCK_INFO->addr.sin_port == 0 && shmSOCK_INFO->err == 0)
        {
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

        if (shmSOCK_INFO->err != 0)
        {
            logger(LOGFILE, "Error: %s", strerror(shmSOCK_INFO->err));
        }

        if (sem_post(&shmSOCK_INFO->sem2) == -1)
        {
            perror("sem_post");
            exit(1);
        }
    }

    if (pthread_join(R, NULL) != 0)
    {
        perror("pthread_join");
        exit(1);
    }

    if (pthread_join(S, NULL) != 0)
    {
        perror("pthread_join");
        exit(1);
    }

    if (pthread_join(G, NULL) != 0)
    {
        perror("pthread_join");
        exit(1);
    }

    // Detach shared memory
    if (shmdt(shmSM) == -1)
    {
        perror("shmdt");
        exit(1);
    }

    if (shmdt(shmSOCK_INFO) == -1)
    {
        perror("shmdt");
        exit(1);
    }

    

    return 0;
}

void *
Rthread (void *arg)
{
    struct SM *shm = (struct SM *)arg;
    fd_set readfds;
    struct timeval tv;
    int maxfd = 0;
    int retval;

    int n;
    char buffer[MAXBUFLEN];
    char message[MAXBUFLEN];

    while (1)
    {
        FD_ZERO(&readfds);
        tv.tv_sec = T;
        tv.tv_usec = 0;

        for (int i = 0; i < N; i++)
        {
            if (shm[i].isFree == 0)
            {
                FD_SET(shm[i].UDPfd, &readfds);
                if (shm[i].UDPfd > maxfd)
                {
                    maxfd = shm[i].UDPfd;
                }
            }
        }

        retval = select(maxfd + 1, &readfds, NULL, NULL, &tv);
        if (retval == -1)
        {
            perror("select");
            exit(1);
        }
        else if (retval)
        {
            for (int i = 0; i < N; i++)
            {
                if (FD_ISSET(shm[i].UDPfd, &readfds))
                {   
                    while (1)
                    {
                        memset(buffer, 0, MAXBUFLEN);
                        n = recvfrom(shm[i].UDPfd, buffer, MAXBUFLEN, 0, (struct sockaddr *)&shm[i].addr, sizeof(shm[i].addr));
                        if (n < 0)
                        {
                            perror("recvfrom");
                            exit(1);
                        }
                        else if (n == 0)
                        {
                            logger(LOGFILE, "Connection closed by peer");
                            break;
                        }
                        else
                        {
                            strcat(message, buffer);
                            if (strstr(message, POSTAMBLE) != NULL)
                            {
                                break;
                            }
                        }
                    }
                    
                    logger(LOGFILE, "Received message: %s", message);
                }
            }
        }
        else
        {
            // check whether a new MTP socket has been created and include it in the read/write set
        }
    }

    return NULL;
}

void *
Sthread (void *arg)
{
}

void *
Gthread (void *arg)
{
}
