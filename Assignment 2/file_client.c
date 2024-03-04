/*
 *  Author:     Prasanna Paithankar
 *  Roll No.:   21CS30065
 *  Course:     Computer Networks Laboratory (CS39006) Spr 2023-24
 *  Date:       18/01/2024

 *  File:       file_client.c
 *  Compile:    gcc -o file_client file_client.c
 *  Run:        ./file_client [-i ip] [-p port] [-h]
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

#define MAXLINE 1024

// Global sockfd for good cleanup
int sockfd;

// Add signal handler
void sigintHandler(int sig_num)
{
    write(1, "\nDo you want to really exit (y/n)? \n", 36);
    char c;
    read(0, &c, 1);
    if (c == 'y' || c == 'Y')
    {
        close(sockfd);
        write(1, "Closing client.\n", 16);
        exit(0);
    }
    else
    {
        write(1, "Continuing.\n", 12);
    }
}

int main(int argc, char *argv[])
{
    
    struct sockaddr_in serv_addr;
    int fd;
    char fileName[100];
    char efileName[100];

    struct sigaction act = {
        .sa_handler = sigintHandler,
        .sa_flags = 0,
        .sa_mask = 0
    };

    if (sigaction(SIGINT, &act, NULL) < 0)
    {
        perror("sigaction");
        exit(1);
    }

    int i;
    char buf[100];

    // Creating socket file descriptor
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Cannot create socket\n");
        exit(0);
    }

    // Filling server information
    serv_addr.sin_family = AF_INET;
    inet_aton("127.0.0.1", &serv_addr.sin_addr);
    serv_addr.sin_port = htons(8881);

    // get the server ip and port no. from command line from flags
    int opt;
    char ip[100];
    while ((opt = getopt(argc, argv, "i:p:q:h")) != -1)
    {
        switch (opt)
        {
            case 'i':
                strcpy(ip, optarg);
                serv_addr.sin_addr.s_addr = inet_addr(ip);
                break;
            case 'p': 
                serv_addr.sin_port = htons(atoi(optarg)); 
                break;
            case 'h':
                write(1, "Usage: server [-i ip] [-p port]\n", 32);
                break;
        }
    }

    write(1, "Computer Networks Lab (CS39006) Spring 2023-24\n", 48);
    write(1, "Assignment 2\n", 13);
    write(1, "Author: Prasanna Paithankar (21CS30065)\n\n", 41);
    write(1, "Usage: ./file_client [-i ip] [-p port] [-h]\n", 44);
    write(1, "*** Press Ctrl+C to exit ***\n\n", 30);

    while (1)
    {
        while (1)
        {
            // Ask for file name
            write(1, "Enter file name: ", 17);
            read(0, fileName, 100);
            fileName[strlen(fileName) - 1] = '\0';

            // Check if file exists
            if ((fd = open(fileName, O_RDONLY, 0666)) < 0)
            {
                perror("File does not exist\n");
                continue;   
            }
            break;
        }

        // Ask for key
        write(1, "Enter key: ", 11);
        read(0, buf, 100);
        int key = htonl(atoi(buf));

        if ((connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr))) < 0)
        {
            perror("Unable to connect to server\n");
            exit(0);
        }

        // Send key
        send(sockfd, &key, sizeof(key), 0);

        //Read the file into buf and send it
        while (1)
        {
            int n = read(fd, buf, 100);
            if (n == 0)
            {
                break;
            }
            send(sockfd, buf, n, 0);
        }

        // Send EOF
        send(sockfd, "0", 1, 0);

        close(fd);

        // Receive the encrypted file
        sprintf(efileName, "%s.enc", fileName);
        fd = open(efileName, O_CREAT|O_WRONLY|O_TRUNC, 0666);
        if (fd < 0)
        {
            perror("open");
            exit(1);
        }

        while (1)
        {
            int n = recv(sockfd, buf, 100, 0);
            if (buf[n - 1] == '0')
            {
                write(fd, buf, n - 1);
                break;
            }
            write(fd, buf, n);
        }
        close(fd);

        char message[MAXLINE];
        sprintf(message, "The file %s has been encrypted and stored as %s\n", fileName, efileName);
        write(1, message, strlen(message));

        close(sockfd);

        // Ask if user wants to continue
        write(1, "Do you want to continue (y/n)? ", 31);
        char c;
        read(0, &c, 1);
        if (c == 'y' || c == 'Y')
        {
            continue;
        }
        else
        {
            write(1, "Closing client.\n", 16);
            break;
        }
    }

    return 0;
}