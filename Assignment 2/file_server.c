/*
 *  Author:     Prasanna Paithankar
 *  Roll No.:   21CS30065
 *  Course:     Computer Networks Laboratory (CS39006) Spr 2023-24
 *  Date:       18/01/2024

 *  File:       file_server.c
 *  Compile:    gcc -o file_server file_server.c
 *  Run:        ./file_server [-p port] [-q qlen] [-h]
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

// Global sockfd for good cleanup
int sockfd, newsockfd;

// Add sig handlers
void sigintHandler(int sig_num)
{
    write(1, "\nDo you want to really exit (y/n)? \n", 36);
    char c;
    read(0, &c, 1);
    if (c == 'y' || c == 'Y')
    {
        close(sockfd);
        close(newsockfd);
        write(1, "Closing server.\n", 16);
        exit(0);
    }
    else
    {
        write(1, "Continuing.\n", 12);
    }
}

// Subroutine to encrypt recieved file
int encryptCeaser(char[], int, struct sockaddr_in);

int main(int argc, char *argv[])
{
    int clilen;
    int qlen = 10; 
    int key;
    struct sockaddr_in cli_addr, serv_addr;
    int filename[100];

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

    // Check if the tmp directory exists
    struct stat st;
    if (stat("tmp", &st) == -1) 
    {
        // Directory does not exist, create it
        if (mkdir("tmp", 0777) != 0) 
        {
            perror("Error creating directory");
            exit(EXIT_FAILURE);
        }
    }

    // Creating socket file descriptor
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Cannot create socket\n");
        exit(0);
    }

    // Filling server information
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(8881);
    
    // set the server port no. and queue length from command line from flags
    int opt;
    char ip[100];
    while ((opt = getopt(argc, argv, "p:q:h")) != -1)
    {
        switch (opt)
        {
            case 'p': 
                serv_addr.sin_port = htons(atoi(optarg)); 
                break;
            case 'q':
                qlen = atoi(optarg);
                break;
            case 'h':
                write(1, "Usage: server [-p port] [-q qlen]\n", 34);
                break;
        }
    }
    
    // Bind the socket to the port
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
    {
		perror("Couldn't bind socket\n");
		exit(0);
	}
    
    write(1, "Computer Networks Lab (CS39006) Spring 2023-24\n", 48);
    write(1, "Assignment 2\n", 13);
    write(1, "Author: Prasanna Paithankar (21CS30065)\n\n", 41);
    write(1, "Usage: ./file_server [-p port] [-q qlen] [-h]\n\n", 48);
    write(1, "*** For closing the server, press Ctrl+C ***\n\n", 46);

    // Start listening
    listen(sockfd, qlen);
    
    while (1)
    {
        clilen = sizeof(cli_addr);

        // Accept a request
		newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen) ;

		if (newsockfd < 0) {
			perror("Couldn't accept request\n");
			exit(0);
		}

        // Fork a child process to handle the request
        if (!fork())
        {
            close(sockfd);  // Close listening socket in this child process

            sprintf(buf, "Connection accepted from %s:%d\n", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
            write(1, buf, strlen(buf));

            sprintf(filename, "./tmp/%s.%d.txt", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
            int tmp_fd = open(filename, O_CREAT|O_WRONLY|O_TRUNC, 0666);    // Create a file clientip.clientport.txt
            if (tmp_fd < 0)
            {
                perror("open");
                exit(1);
            }

			for(i=0; i < 100; i++) buf[i] = '\0';
            size_t count;
            
            // Get the key
            recv(newsockfd, &key, sizeof(key), 0);
            key = ntohl(key);

            // Recieve the file
            while (count = recv(newsockfd, buf, 100, 0))
            {
                // If last char is 0, break
                if (buf[count - 1] == '0')
                {
                    write(tmp_fd, buf, count - 1);
                    break;
                }
                write(tmp_fd, buf, count);
            }
            close(tmp_fd);

            // Encrypt the file
            int encrypted_fd = encryptCeaser(filename, key, cli_addr);
            sprintf(filename, "./tmp/%s.%d.txt.enc", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));

            // Send the file
            encrypted_fd = open(filename, O_RDONLY, 0666);
            while (count = read(encrypted_fd, buf, 100))
            {
                send(newsockfd, buf, count, 0);
            }
            send(newsockfd, "0", 1, 0);
            close(encrypted_fd);
			close(newsockfd);

            sprintf(buf, "Request from %s:%d served\n", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
            write(1, buf, strlen(buf));

			exit(0);
        }
        
        // Close the socket in the parent process
        close(newsockfd);
    }
    
    return 0;
}

int encryptCeaser(char filename[100], int key, struct sockaddr_in cli_addr)
{
    // Create a file clientip.clientport.txt.enc
    char buf[100];
    sprintf(buf, "./tmp/%s.%d.txt.enc", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
    int enc_fd = open(buf, O_CREAT|O_WRONLY|O_TRUNC, 0666);
    if (enc_fd < 0)
    {
        perror("open");
        exit(1);
    }

    // Read from fd char by char and write to enc_fd
    int fd = open(filename, O_RDONLY, 0666);
    char c;
    while (read(fd, &c, 1))
    {
        // Encrypt Uppercase letters
        if (isupper(c))
        {
            c = (char)((int)(c + key - 65) % 26 + 65);
        }
        // Encrypt Lowercase letters
        else if (islower(c))
        {
            c = (char)((int)(c + key - 97) % 26 + 97);
        }

        write(enc_fd, &c, 1);
        
    }
    close(fd);
    close(enc_fd);

    return enc_fd;
}

