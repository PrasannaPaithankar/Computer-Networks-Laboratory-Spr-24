/*
 *  Author:     Prasanna Paithankar
 *  Roll No.:   21CS30065
 *  Course:     Computer Networks Laboratory (CS39006) Spr 2023-24
 *  Date:       07/01/2024

 *  File:       wordclient.c
 *  Compile:    gcc -o wordclient wordclient.c
 *  Run:        ./wordclient -i <server-ip> -p <Port no.> -h
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define MAXLINE 1024

int main(int argc, char *argv[]) {
    int sockfd;
    char buffer[MAXLINE];
    char filename[100];
    FILE *fp;
    struct sockaddr_in servaddr;
    int opt;


    // Creating socket file descriptor
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));

    // Filling server information
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(8881);
    servaddr.sin_addr.s_addr = INADDR_ANY;

    // get the server ip and port no. from command line from flags
    char ip[100];
    while((opt = getopt(argc, argv, "i:p:h")) != -1) 
    {
        switch(opt) 
        {
            case 'i':
                strcpy(ip, optarg);
                servaddr.sin_addr.s_addr = inet_addr(ip);
                break;
            case 'p':
                servaddr.sin_port = htons(atoi(optarg));
                break;
            case 'h':
                printf("Usage: %s -i <server-ip> -p <Port no.>\n\n", argv[0]);
                break;
        }
    }

    int n, len;
    printf("Computer Networks Lab (CS39006) Spring 2023-24\n");
    printf("Assignment 1B\n");
    printf("Author: Prasanna Paithankar (21CS30065)\n\n");
    printf("Usage: %s -i <server-ip> -p <Port no.>\n", argv[0]);
    printf("For help use -h flag\n\n");

    while (1)
    {
        printf("\nEnter filename: ");
        scanf("%s", filename);

        sendto(sockfd, (const char *)filename, strlen(filename), MSG_CONFIRM, (const struct sockaddr *) &servaddr, sizeof(servaddr));

        // Recieve the file opening status from server
        n = recvfrom(sockfd, (char *)buffer, MAXLINE, MSG_WAITALL, (struct sockaddr *) &servaddr, &len);
        buffer[n] = '\0';
        printf("Server : %s\n", buffer);
        char notfound[MAXLINE];
        sprintf(notfound, "NOTFOUND %s", filename);
        if (strcmp(buffer, notfound) == 0) {
            printf("File %s Not Found.\n", filename);
            close(sockfd);
            exit(EXIT_FAILURE);
        }
        else if (strncmp(buffer, "FOUND", 5) == 0) {
            printf("File found on server.\n");
        }
        else 
        {
            printf("Unknown response from server.\n");
            close(sockfd);
            exit(EXIT_FAILURE);
        }

        // Recieve the file contents from server
        n = recvfrom(sockfd, (char *)buffer, MAXLINE, MSG_WAITALL, (struct sockaddr *) &servaddr, &len);
        buffer[n] = '\0';

        if (strcmp(buffer, "HELLO") || strcmp(buffer, "HELLO\n") || strcmp(buffer, "HELLO\r\n") || strcmp(buffer, "HELLO\r"))
        {
            // Create a file to write
            printf("Enter Local Filename: ");
            scanf("%s", filename);
            fp = fopen(filename, "w");
            if (fp == NULL)
            {
                perror("Error opening file");
                close(sockfd);
                exit(EXIT_FAILURE);
            }
            printf("File opened.\n");
        }
        else
        {
            printf("Unknown response from server.\n");
            close(sockfd);
            exit(EXIT_FAILURE);
        }

        int i = 1;
        while (i++)
        {
            // Send WORDi to the server
            sprintf(buffer, "WORD%d", i - 1);
            sendto(sockfd, (const char *)buffer, strlen(buffer), MSG_CONFIRM, (const struct sockaddr *) &servaddr, sizeof(servaddr));

            n = recvfrom(sockfd, (char *)buffer, MAXLINE, MSG_WAITALL, (struct sockaddr *) &servaddr, &len);
            buffer[n] = '\0';
            if (strcmp(buffer, "END") == 0 || strcmp(buffer, "END\n") == 0 || strcmp(buffer, "END\r\n") == 0 || strcmp(buffer, "END\r") == 0) 
            {
                printf("File has been saved.\n");
                break;
            }
            fprintf(fp, "%s", buffer);
        }

        printf("Do you want make another request (y/n): ");
        char ch;
        scanf(" %c", &ch);
        if (ch == 'n' || ch == 'N')
        {
            printf("Exiting...\n");
            break;
        }
    }

    // Close the file
    fclose(fp);
    close(sockfd);
    return 0;
}