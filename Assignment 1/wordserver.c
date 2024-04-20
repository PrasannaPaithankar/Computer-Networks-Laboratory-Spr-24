/*
 *  Author:     Prasanna Paithankar
 *  Roll No.:   21CS30065
 *  Course:     Computer Networks Laboratory (CS39006) Spr 2023-24
 *  Date:       07/01/2024

 *  File:       wordserver.c
 *  Compile:    gcc -o wordserver wordserver.c
 *  Run:        ./wordserver -p <Port no.> -h
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define MAXLINE 1024

// Override SIGINT handler
void sigintHandler(int sig_num)
{
    write(0, "\nDo you want to really exit (y/n)? \n", 36);
    char c;
    read(0, &c, 1);
    if (c == 'y' || c == 'Y')
    {
        printf("Closing server.\n");
        exit(0);
    }
    else
    {
        printf("Continuing.\n");
    }
}

int main(int argc, char *argv[])
{
    int sockfd;
    char buffer[MAXLINE];
    char *fbuffer;
    size_t flen = 0;
    ssize_t fread;
    char filename[100];
    char wordi[MAXLINE];
    FILE *fp;
    struct sockaddr_in servaddr, cliaddr;

    // Override SIGINT handler
    signal(SIGINT, sigintHandler);

    // Creating socket file descriptor
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));
    memset(&cliaddr, 0, sizeof(cliaddr));

    // Filling server information
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(8881); // Default to port 8881 if no port is specified
    servaddr.sin_addr.s_addr = INADDR_ANY;

    // get the server port no. from command line from flags
    int opt;
    while ((opt = getopt(argc, argv, "p:h")) != -1)
    {
        switch (opt)
        {
        case 'p':
            servaddr.sin_port = htons(atoi(optarg));
            break;
        case 'h':
            printf("Usage: %s -p <Port no.>\n\n", argv[0]);
            break;
        }
    }

    // Bind the socket with the server address
    if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    size_t n;
    int len;

    len = sizeof(cliaddr); // len is value/resuslt

    printf("Computer Networks Lab (CS39006) Spring 2023-24\n");
    printf("Assignment 1B\n");
    printf("Author: Prasanna Paithankar (21CS30065)\n\n");
    printf("Usage: %s -p <Port no.>\n", argv[0]);
    printf("For help use -h flag\n\n");
    printf("***For closing the server or abandoning the running request, press Ctrl+C\n");

    while (1)
    {
        // Recieve filename from client
        printf("\nWaiting for client request...\n");
        n = recvfrom(sockfd, (char *)filename, MAXLINE, MSG_WAITALL, (struct sockaddr *)&cliaddr, &len);
        filename[n] = '\0';

        printf("Client (%s) requested file: %s\n", inet_ntoa(cliaddr.sin_addr), filename);

        // Open the file, if the file is not found then send NOTFOUND [filename], else send FOUND
        fp = fopen(filename, "r");
        if (fp == NULL)
        {
            sprintf(buffer, "NOTFOUND %s", filename);
            sendto(sockfd, (const char *)buffer, strlen(buffer), MSG_CONFIRM, (const struct sockaddr *)&cliaddr, len);
            perror("File not found");
            exit(EXIT_FAILURE);
        }
        else
        {
            sendto(sockfd, (const char *)"FOUND", strlen("FOUND"), MSG_CONFIRM, (const struct sockaddr *)&cliaddr, len);
        }

        // read the first line of the file and send it to client
        fread = getline(&fbuffer, &flen, fp);
        fbuffer[flen] = '\0';
        sendto(sockfd, (const char *)fbuffer, strlen(fbuffer), MSG_CONFIRM, (const struct sockaddr *)&cliaddr, len);

        int i = 1;
        while (i++)
        {
            // Recieve WORDi from client
            n = recvfrom(sockfd, (char *)buffer, MAXLINE, MSG_WAITALL, (struct sockaddr *)&cliaddr, &len);
            buffer[n] = '\0';

            sprintf(wordi, "WORD%d", i - 1);
            if (strcmp(buffer, wordi) != 0)
            {
                fprintf(stderr, "Error: Client sent wrong word request.\n");
                break;
            }

            // Send next line of file to client
            fread = getline(&fbuffer, &flen, fp);
            fbuffer[flen] = '\0';
            sendto(sockfd, (const char *)fbuffer, strlen(fbuffer), MSG_CONFIRM, (const struct sockaddr *)&cliaddr, len);

            // if buffer reads END then break
            if (strcmp(fbuffer, "END") == 0 || strcmp(fbuffer, "END\n") == 0 || strcmp(fbuffer, "END\r\n") == 0 || strcmp(fbuffer, "END\r") == 0)
            {
                printf("File sent to client (%s)\n", inet_ntoa(cliaddr.sin_addr));
                break;
            }
        }
    }

    fclose(fp);
    close(sockfd);
    return 0;
}
