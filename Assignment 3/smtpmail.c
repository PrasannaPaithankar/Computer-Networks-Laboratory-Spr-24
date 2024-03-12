/*
 *  Author:     Prasanna Paithankar
 *  Roll No.:   21CS30065
 *  Course:     Computer Networks Laboratory (CS39006) Spr 2023-24
 *  Date:       01/02/2024

 *  File:       smtpmail.c
 *  Compile:    gcc -o smtpmail smtpmail.c [-DV for verbose]
 *  Run:        ./smtpmail [my_port] [qlen]
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h> 
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/stat.h>
#include <time.h>

// Signal handler for SIGINT
void sigintHandler(int sig_num)
{
    write(1, "\nSMTP Server shutting down...\n", 30);
    write(1, "\nAuthor: Prasanna Paithankar\n", 29);
    exit(0);
}

int main(int argc, char *argv[])
{
    int sockfd, connfd, len, n;
    int qlen = argc > 2 ? atoi(argv[2]) : 100;  // qlen is the length of the queue for pending connections
    struct sockaddr_in servaddr, cliaddr;       // servaddr is the server address, cliaddr is the client address
    char buff[4096];
    char from[256], to[256];
    char mail[4096];

    // Register signal handler for SIGINT
    signal(SIGINT, sigintHandler);

    // Create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set socket options
    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("Setsockopt failed");
        exit(EXIT_FAILURE);
    }

    // Bind socket to port
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    if (argc > 1)
        servaddr.sin_port = htons(atoi(argv[1]));
    else
        servaddr.sin_port = htons(25);
        
    if (bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) 
    {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for connections
    if (listen(sockfd, qlen) < 0) 
    {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("SMTP Server listening on port %d\n", ntohs(servaddr.sin_port));
    printf("Press Ctrl+C to exit\n\n");

    // Concurrent server
    while (1) 
    {
        // Accept connection
        len = sizeof(cliaddr);
        connfd = accept(sockfd, (struct sockaddr *)&cliaddr, &len);
        if (connfd < 0) {
            perror("Accept failed");
            exit(EXIT_FAILURE);
        }

        if (!fork())
        {
            // Child connected to client
            close(sockfd);
            FILE *fp;
            int flag = 0;

            // Send 220
            write(connfd, "220 <smtp.pipi.ac.in> Service ready\r\n", 37);

            printf("Connected to client %s:%d\n", inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port));

            // Recieve HELO
            while (n = read(connfd, buff, 4096))
            {
                if (strncmp(buff, "HELO", 4) == 0)
                {
                    write(connfd, "250 OK smtp.pipi.ac.in\r\n", 24);
                    break;
                }
                else
                {
                    write(connfd, "500 HELO not recognized\r\n", 25);
                    flag = 1;
                    printf("HELO not recognized\n\n");
                } 
            }
            memset(buff, 0, sizeof(buff));

            if (flag)
            {
                close(connfd);
                exit(0);
            }
            #ifdef V
                printf("HELO received\n");
            #endif

            // Recieve MAIL FROM
            
            while (n = read(connfd, buff, 4096))
            {
                if (strncmp(buff, "MAIL FROM:", 10) == 0)
                {
                    if (strstr(buff, "\r\n") != NULL)
                    {
                        strcpy(from, buff + 11);

                        // Check if from email exists in user.txt
                        if (from[strlen(from) - 2] == '\r')
                            from[strlen(from) - 2] = '\0';
                        else
                            from[strlen(from) - 1] = '\0';
                        char fromcpy[256];
                        strcpy(fromcpy, from);
                        char *pos = strchr(from, '>');
                        if (pos != NULL)
                            *pos = '\0';
                        strtok(fromcpy, "@");

                        char filename[] = "./user.txt";

                        fp = fopen(filename, "r");
                        if (fp == NULL)
                        {
                            perror("File open failed");
                            exit(EXIT_FAILURE);
                        }
                        char line[256];
                        flag = 1;
                        while (fgets(line, sizeof(line), fp))
                        {
                            if (strcmp(strtok(line, "@"), fromcpy) == 0)
                            {
                                flag = 0;
                                break;
                            }
                        }
                        fclose(fp);

                        if (flag)
                        {
                            write(connfd, "550 No such user\r\n", 18);
                            printf("No such user\n\n");
                            break;
                        }

                        // send 250 <from> OK
                        write(connfd, "250 ", 4);
                        write(connfd, from, strlen(from));
                        write(connfd, " OK\r\n", 5);
                        break;
                    }
                }
                else
                {
                    write(connfd, "500 MAIL FROM not recognized\r\n", 30);
                    flag = 1;
                    printf("MAIL FROM not recognized\n\n");
                } 
            }
            memset(buff, 0, sizeof(buff));

            if (flag)
            {
                close(connfd);
                exit(0);
            }
            #ifdef V
                printf("MAIL FROM received %s\n", from);
            #endif

            // Recieve RCPT TO
            while (n = read(connfd, buff, 4096))
            {
                if (strncmp(buff, "RCPT TO:", 8) == 0)
                {
                    strcpy(to, buff + 9);

                    // Check if from email exists in user.txt
                    if (to[strlen(to) - 2] == '\r')
                        to[strlen(to) - 2] = '\0';
                    else
                        to[strlen(to) - 1] = '\0';
                    char tocpy[256];
                    strcpy(tocpy, to);
                    char *pos = strchr(to, '>');
                    if (pos != NULL)
                        *pos = '\0';
                    strtok(tocpy, "@");

                    char filename[] = "./user.txt";

                    fp = fopen(filename, "r");
                    if (fp == NULL)
                    {
                        perror("File open failed");
                        exit(EXIT_FAILURE);
                    }
                    flag = 1;
                    char line[256];
                    while (fgets(line, sizeof(line), fp))
                    {
                        if (strcmp(strtok(line, "@"), tocpy) == 0)
                        {
                            flag = 0;
                            break;
                        }
                    }
                    fclose(fp);
                    if (flag)
                    {
                        write(connfd, "550 No such user\r\n", 18);
                        printf("No such user\n\n");
                        break;
                    }

                    // send 250 <to> OK
                    write(connfd, "250 ", 4);
                    write(connfd, to, strlen(to));
                    write(connfd, " OK\r\n", 5);
                    break;
                }
                else
                {
                    write(connfd, "500 RCPT TO not recognized\r\n", 28);
                    flag = 1;
                    printf("RCPT TO not recognized\n\n");
                } 
            }
            memset(buff, 0, sizeof(buff));

            #ifdef V
                printf("RCPT TO received %s\n", to);
            #endif

            if (flag)
            {
                close(connfd);
                exit(0);
            }

            // Recieve DATA
            while (n = read(connfd, buff, 4096))
            {
                if (strncmp(buff, "DATA", 4) == 0)
                {
                    write(connfd, "354 Enter mail, end with \".\" on a line by itself\r\n", 50);
                    break;
                }
                else
                {
                    write(connfd, "500 DATA not recognized\r\n", 25);
                    flag = 1;
                    printf("DATA not recognized\n\n");
                } 
            }
            memset(buff, 0, sizeof(buff));

            #ifdef V
                printf("DATA received\n");
            #endif

            if (flag)
            {
                close(connfd);
                exit(0);
            }

            // Recieve mail
            int i = 0;
            memset(mail, 0, sizeof(mail));
            memset(buff, 0, sizeof(buff));
            while (n = read(connfd, buff, 4096))
            {
                if (strstr(buff, "\r\n.\r\n") != NULL)
                {
                    strcat(mail, buff);
                    break;
                }
                else
                {
                    strcat(mail, buff);
                    memset(buff, 0, sizeof(buff));
                } 
            }
            memset(buff, 0, sizeof(buff));

            // Send 250 OK
            write(connfd, "250 OK Message accepted for delivery\r\n", 38);

            // Recieve QUIT
            while (n = read(connfd, buff, 4096))
            {
                if (strncmp(buff, "QUIT", 4) == 0)
                {
                    write(connfd, "221 smtp.pipi.ac.in closing connection\r\n", 40);
                    break;
                }
                else
                {
                    write(connfd, "500 QUIT not recognized\r\n", 25);
                    flag = 1;
                    printf("QUIT not recognized\n\n");
                } 
            }
            memset(buff, 0, sizeof(buff));

            #ifdef V
                printf("QUIT received\n");
            #endif

            if (flag)
            {
                close(connfd);
                exit(0);
            }

            char *subjectPos = strstr(mail, "Subject:");
            // Calculate the length of the "Subject" line
            size_t subjectLen = strcspn(subjectPos, "\n");

            // Calculate the position to insert the "Received" header
            char *insertPos = subjectPos + subjectLen;

            // Get the current time
            time_t t;
            time(&t);
            struct tm *tm_info = localtime(&t);
            char receivedHeader[100];
            strftime(receivedHeader, sizeof(receivedHeader), "\nReceived: %Y-%m-%d : %H : %M", tm_info);

            // Insert the "Received" header after the "Subject" line
            memmove(insertPos + strlen(receivedHeader), insertPos, strlen(insertPos) + 1);
            memcpy(insertPos, receivedHeader, strlen(receivedHeader));

            // append mail to file in ./<username>/mymailbox
            char *username = strtok(to, "@");
            if (username == NULL) 
            {
                perror("No username flag");
                exit(EXIT_FAILURE);
            }
            char wfilename[255];
            sprintf(wfilename, "./%s/mymailbox", username);

            #ifdef V
                printf("Writing mail to file %s\n", wfilename);
            #endif
            
            fp = fopen(wfilename, "a");
            if (fp == NULL)
            {
                perror("File open failed");
                flag = 0;
            }
            fprintf(fp, "%s", mail);
            fclose(fp);

            printf("Successfully served client %s:%d\n\n", inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port));

            // Close connection
            close(connfd);
            exit(0);
        }
        close(connfd);
    }

    return 0;
}
