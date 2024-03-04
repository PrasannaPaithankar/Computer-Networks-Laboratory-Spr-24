/*
 *  Author:     Prasanna Paithankar
 *  Roll No.:   21CS30065
 *  Course:     Computer Networks Laboratory (CS39006) Spr 2023-24
 *  Date:       09/02/2024
 *  File:       peer.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

// P2P protocol
int main(int argc, char *argv[])
{
    int sockfd, connfd[2], len;
    struct sockaddr_in servaddr, cliaddr;
    connfd[0] = -1;
    connfd[1] = -1;
    int fduser[2];
    fduser[0] = -1;
    fduser[1] = -1;

    struct user_info
    {
        char name[20];
        char ip[20];
        int port;
    } user_info[3];

    // The user_info table is initialized with the information of the friends. 
    strcpy(user_info[0].name, "user1");
    strcpy(user_info[0].ip, "127.0.0.1");
    user_info[0].port = 50000;

    strcpy(user_info[1].name, "user2");
    strcpy(user_info[1].ip, "127.0.0.1");
    user_info[1].port = 50001;

    strcpy(user_info[2].name, "user3");
    strcpy(user_info[2].ip, "127.0.0.1");
    user_info[2].port = 50002;

    // Create a socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
    {
        printf("Socket creation failed...\n");
        exit(0);
    }

    // Initialize the server address
    bzero(&servaddr, sizeof(servaddr));

    // Assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(user_info[atoi(argv[1])-1].port);

    printf("Running on port %d\n", user_info[atoi(argv[1])-1].port);

    // Bind the socket with the server address
    if ((bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0)
    {
        printf("Socket bind failed...\n");
        exit(0);
    }

    // Now server is ready to listen and verification
    if ((listen(sockfd, 5)) != 0)
    {
        printf("Listen failed...\n");
        exit(0);
    }

    // Set timeout for the sockfd and stdin
    struct timeval tv;
    tv.tv_sec = 300;
    tv.tv_usec = 0;
    fd_set readfds;

    int flag = 0;

    while (1)
    {
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
        FD_SET(0, &readfds);

        int maxfd;

        if (flag == 0)
        {
            maxfd = sockfd > 0 ? sockfd : 0;
        }
        if (flag >= 1)
        {
            maxfd = connfd[1] > connfd[0] ? (connfd[1] > sockfd ? connfd[1] : sockfd) : connfd[0] > sockfd ? connfd[0] : sockfd;
        }
        int ret = select(maxfd + 1, &readfds, NULL, NULL, &tv);

        if (ret < 0)
        {
            printf("Select error\n");
            exit(0);
        }
        else if (ret == 0)
        {
            continue;
        }
        else
        {
            if (FD_ISSET(sockfd, &readfds))
            {
                // Recieve the message from the client
                int curr;
                len = sizeof(cliaddr);
                if ((connfd[0] = accept(sockfd, (struct sockaddr *)&cliaddr, &len)) < 0)
                {
                    if ((connfd[1] = accept(sockfd, (struct sockaddr *)&cliaddr, &len)) < 0)
                    {
                        printf("Server accept failed...\n");
                        exit(0);
                    }
                    else
                    {
                        curr = 1;
                    }
                }
                else
                {
                    curr = 0;
                }
                {
                    if (connfd[0] + connfd[1] == -2)
                    {
                        flag = 0;
                    }
                    else if (connfd[0] + connfd[1] == -1)
                    {
                        flag = 1;
                    }
                    else if (connfd[0] + connfd[1] == 0)
                    {
                        flag = 2;
                    }
                }

                char buffer[1024];
                char message[1024];
                memset(buffer, 0, sizeof(buffer));
                memset(message, 0, sizeof(message));
                while (1)
                {
                    int n = recv(connfd[curr], buffer, sizeof(buffer), 0);
                    strcat(message, buffer);
                    memset(buffer, 0, sizeof(buffer));
                    if (message[strlen(message) - 1] == '\n')
                    {
                        break;
                    }
                }

                printf("Message recieved from %s\n", message);
                char *friendname = strtok(message, " ");
                char *messagep = strtok(NULL, " ");
                fduser[curr] = friendname[5] - '0';
            }

            if (FD_ISSET(connfd[0], &readfds))
            {
                // Recieve the message from the client
                char buffer[1024];
                char message[1024];
                memset(buffer, 0, sizeof(buffer));
                memset(message, 0, sizeof(message));
                while (1)
                {
                    int n = recv(connfd[0], buffer, sizeof(buffer), 0);
                    strcat(message, buffer);
                    memset(buffer, 0, sizeof(buffer));
                    if (message[strlen(message) - 1] == '\n')
                    {
                        break;
                    }
                }

                printf("Message recieved from %s\n", message);
            }

            if (FD_ISSET(connfd[1], &readfds))
            {
                // Recieve the message from the client
                char buffer[1024];
                char message[1024];
                memset(buffer, 0, sizeof(buffer));
                memset(message, 0, sizeof(message));
                while (1)
                {
                    int n = recv(connfd[1], buffer, sizeof(buffer), 0);
                    strcat(message, buffer);
                    memset(buffer, 0, sizeof(buffer));
                    if (message[strlen(message) - 1] == '\n')
                    {
                        break;
                    }
                }

                printf("Message recieved from %s\n", message);
            }

            if (FD_ISSET(0, &readfds))
            {
                char buffer[1024];
                fgets(buffer, sizeof(buffer), stdin);

                // message of format friendname/message
                char *friendname = strtok(buffer, "/");
                char *message = strtok(NULL, "/");

                // trim the newline character
                message[strlen(message) - 1] = '\0';

                // Find port of friend
                int port, i;
                for (i = 0; i < 3; i++)
                {
                    if (strcmp(user_info[i].name, friendname) == 0)
                    {
                        port = user_info[i].port;
                        break;
                    }
                }

                // Send the message to the friend after checking if the there was no previous connection existing
                if (fduser[0] == friendname[5] - '0')
                {
                    i = 0;
                }
                else if (fduser[1] == friendname[5] - '0')
                {
                    i = 1;
                }

                else 
                {
                    if (fduser[0] == -1)
                    {
                        i = 0;
                    }
                    else if (fduser[1] == -1)
                    {
                        i = 1;
                    }
                }

                if (connfd[i] == -1)
                {
                    connfd[i] = socket(AF_INET, SOCK_STREAM, 0);
                    if (connfd[i] == -1)
                    {
                        printf("Socket creation failed...\n");
                        break;
                    }
                    
                    bzero(&servaddr, sizeof(servaddr));

                    // Assign IP, PORT
                    servaddr.sin_family = AF_INET;
                    servaddr.sin_addr.s_addr = inet_addr(user_info[i].ip);
                    servaddr.sin_port = htons(port);

                    // Connect the client socket to server socket
                    if (connect(connfd[i], (struct sockaddr *)&servaddr, sizeof(servaddr)) != 0)
                    {
                        printf("Connection with the server failed...\n");
                        break;
                    }
                }

                // Send the message to the friend
                char toSend[1024];
                sprintf(toSend, "user_%s: %s\n", argv[1], message);
                send(connfd[i], toSend, sizeof(toSend), 0);

                // Reset the buffer
                bzero(buffer, sizeof(buffer));
            }
        }
    }
    close(sockfd);
    if (connfd[0] != -1)
        close(connfd[0]);
    if (connfd[1] != -1)
        close(connfd[1]);
    if (connfd[2] != -1)
        close(connfd[2]);
    if (connfd[0] != -1)
        close(connfd[0]);
    if (connfd[1] != -1)
        close(connfd[1]);
    return 0;
}



