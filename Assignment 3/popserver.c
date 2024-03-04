/*
 *  Author:     Prasanna Paithankar
 *  Roll No.:   21CS30065
 *  Course:     Computer Networks Laboratory (CS39006) Spr 2023-24
 *  Date:       

 *  File:       popserver.c
 *  Compile:    gcc -o popserver popserver.c
 *  Run:        ./popserver [my_port] [qlen]
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

// Signal handler for SIGINT
void sigintHandler(int sig_num)
{
    write(1, "\nPOP3 Server shutting down...\n", 30);
    write(1, "\nAuthor: Prasanna Paithankar\n", 29);
    exit(0);
}

// Check if user exists
int checkExistence(char *username)
{
    char filename[272];
    sprintf(filename, "./%s/mymailbox", username);
    FILE *fp = fopen(filename, "r");
    if (fp != NULL)
    {
        fclose(fp);
        return 1;
    }
    return 0;
}

// Check if username and password are correct
int authenticate(char username[], char password[])
{
    char filename[272];
    sprintf(filename, "./user.txt");
    // the file format in user.txt is username password
    FILE *fp = fopen(filename, "r");
    if (fp != NULL)
    {
        char *user, *pass;
        char line[512];
        while (fgets(line, sizeof(line), fp))
        {
            user = strtok(line, " ");
            pass = strtok(NULL, " ");
            if (strncmp(user, username, strlen(username)) == 0)
            {
                if (strncmp(pass, password, strlen(password)) == 0)
                {
                    fclose(fp);
                    return 1;
                }
            }
        }
    }
    return 0;
}

int main(int argc, char *argv[])
{
    int sockfd, connfd, len, n;
    int qlen = argc > 2 ? atoi(argv[2]) : 100;  // qlen is the maximum length of the queue of pending connections
    struct sockaddr_in servaddr, cliaddr;       // server and client address
    char buff[4096];
    char mail[4096];
    char username[256], password[256];
    int delMarked[256];                         // to mark mails for deletion

    // Register signal handler for SIGINT
    signal(SIGINT, sigintHandler);

    // Create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
    {
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
        servaddr.sin_port = htons(110);
        
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

    printf("POP3 Server listening on port %d\n", ntohs(servaddr.sin_port));
    printf("Press Ctrl+C to exit\n\n");

    while (1) 
    {
        // Accept connection
        len = sizeof(cliaddr);
        connfd = accept(sockfd, (struct sockaddr *)&cliaddr, &len);
        if (connfd < 0) 
        {
            perror("Accept failed");
            exit(EXIT_FAILURE);
        }
        
        if (!fork())
        {
            // Child process
            close(sockfd);
            int authenticated = 0;
            memset(delMarked, 0, sizeof(delMarked));

            printf("Connected to client: %s:%d\n", inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port));

            // Send welcome message
            n = write(connfd, "+OK POP3 server ready\r\n", 23);
            if (n < 0) {
                perror("Write failed");
                exit(EXIT_FAILURE);
            }
            #ifdef V
                printf("Server: +OK POP3 server ready\n");
            #endif

            /*
             *  State encoding
             *  0 - Authorization
             *  1 - Transaction
             *  2 - Update
             */
            int state = 0;
            int flag = 0;

            // State space
            while (1)
            {
                switch(state)
                {
                    // Authorization
                    case 0:
                        // USER
                        char cmd[256];
                        memset(cmd, 0, sizeof(cmd));
                        memset(buff, 0, sizeof(buff));
                        while (1)
                        {
                            n = read(connfd, cmd, 256);
                            #ifdef V
                                printf("Client: %s", cmd);
                            #endif
                            strcat(buff, cmd);
                            if (n < 0) {
                                perror("Read failed");
                                exit(EXIT_FAILURE);
                            }
                            if (strstr(buff, "\r\n") != NULL)
                            {
                                break;
                            }
                            memset(cmd, 0, sizeof(cmd));
                        }

                        if (strncmp(buff, "USER", 4) == 0)
                        {
                            sscanf(buff, "USER %s", username);
                            // check if user exists in user.txt
                            if (checkExistence(username))
                            {
                                n = write(connfd, "+OK User exists\r\n", 17);
                                if (n < 0) {
                                    perror("Write failed");
                                    exit(EXIT_FAILURE);
                                }
                            }
                            else
                            {
                                n = write(connfd, "-ERR User does not exist\r\n", 25);
                                if (n < 0) {
                                    perror("Write failed");
                                    exit(EXIT_FAILURE);
                                }
                                flag = 1;
                            }
                        }
                        if (flag)
                        {
                            state = 2;
                            break;
                        }

                        //  PASS
                        memset(buff, 0, sizeof(buff));
                        n = read(connfd, buff, 4096);
                        if (n < 0) {
                            perror("Read failed");
                            exit(EXIT_FAILURE);
                        }
                        buff[n] = '\0';
                        #ifdef V
                            printf("Client: %s", buff);
                        #endif

                        if (strncmp(buff, "PASS", 4) == 0)
                        {
                            sscanf(buff, "PASS %s", password);
                            // check if password is correct
                            if (authenticate(username, password))
                            {
                                n = write(connfd, "+OK User authenticated\r\n", 24);
                                if (n < 0) {
                                    perror("Write failed");
                                    exit(EXIT_FAILURE);
                                }
                                authenticated = 1;
                            }
                            else
                            {
                                n = write(connfd, "-ERR Authentication failed\r\n", 29);
                                if (n < 0) 
                                {
                                    perror("Write failed");
                                    exit(EXIT_FAILURE);
                                }
                                flag = 1;
                            }
                        }
                        if (flag)
                        {
                            state = 2;
                            break;
                        }

                        if (authenticated)
                            state = 1;
                        break;

                    case 1:
                        while (1)
                        {
                            // STAT
                            memset(buff, 0, sizeof(buff));
                            n = read(connfd, buff, 4096);
                            if (n < 0) 
                            {
                                perror("Read failed");
                                exit(EXIT_FAILURE);
                            }
                            buff[n] = '\0';
                            #ifdef V
                                printf("Client: %s", buff);
                            #endif

                            int count = 0;
                            if (strncmp(buff, "STAT", 4) == 0)
                            {
                                // get number of mails
                                char filename[272];
                                sprintf(filename, "./%s/mymailbox", username);
                                FILE *fp = fopen(filename, "r");
                                if (fp != NULL)
                                {
                                    while (fgets(mail, sizeof(mail), fp))
                                    {
                                        if (strncmp(mail, "From:", 5) == 0)
                                            count++;
                                        memset(mail, 0, sizeof(mail));
                                    }
                                    fclose(fp);
                                }
                                sprintf(mail, "+OK %d %d\r\n", count, 0);
                                n = write(connfd, mail, strlen(mail));
                                if (n < 0) {
                                    perror("Write failed");
                                    exit(EXIT_FAILURE);
                                }
                            }

                            // LIST
                            else if (strncmp(buff, "LIST", 4) == 0)
                            {
                                // get number of mails
                                char filename[272];
                                sprintf(filename, "./%s/mymailbox", username);
                                FILE *fp = fopen(filename, "r");
                                if (fp == NULL)
                                {
                                    n = write(connfd, "-ERR\r\n", 6);
                                    if (n < 0) {
                                        perror("Write failed");
                                        exit(EXIT_FAILURE);
                                    }
                                }
                                else
                                {
                                    n = write(connfd, "+OK\r\n", 5);
                                    if (n < 0) {
                                        perror("Write failed");
                                        exit(EXIT_FAILURE);
                                    }
                                    int id = 1;
                                    memset(mail, 0, sizeof(mail));
                                    
                                    char from[256], date[256], subject[256];
                                    
                                    while (fgets(mail, sizeof(mail), fp))
                                    {
                                        // Append to mail starting from FROM till Subject end
                                        if (strncmp(mail, "From:", 5) == 0)
                                        {
                                            sscanf(mail, "From: %s", from);
                                            
                                            //  To skip To
                                            memset(mail, 0, sizeof(mail));
                                            fgets(mail, sizeof(mail), fp);

                                            memset(mail, 0, sizeof(mail));
                                            fgets(mail, sizeof(mail), fp);
                                            strcpy(subject, mail + 9);
                                            // remove \n from subject
                                            subject[strlen(subject) - 1] = '\0';

                                            memset(mail, 0, sizeof(mail));
                                            fgets(mail, sizeof(mail), fp);
                                            strcpy(date, mail + 10);
                                            // remove \n from date
                                            date[strlen(date) - 1] = '\0';

                                            memset(mail, 0, sizeof(mail));
                                            #ifdef V
                                                printf("Server: %d ", id);
                                                printf("Server: %s ", from);
                                                printf("Server: %s ", date);
                                                printf("Server: %s\n", subject);
                                            #endif
                                            sprintf(mail, "%d %s %s %s\r\n", id, from, date, subject);
                                            n = write(connfd, mail, strlen(mail));
                                            if (n < 0) {
                                                perror("Write failed");
                                                exit(EXIT_FAILURE);
                                            }

                                            id++;
                                        }
                                    }
                                    fclose(fp);
                                    if (id == 1)
                                    {
                                        #ifdef V
                                            printf("Server: No mails found\n");
                                        #endif
                                        n = write(connfd, "Empty Mailbox\r\n", 15);
                                        if (n < 0) 
                                        {
                                            perror("Write failed");
                                            exit(EXIT_FAILURE);
                                        }
                                    }
                                    write(connfd, ".\r\n", 3);
                                }
                            }
                                
                            // RETR
                            else if (strncmp(buff, "RETR", 4) == 0)
                            {
                                int id;
                                sscanf(buff, "RETR %d", &id);
                                char filename[272];
                                sprintf(filename, "./%s/mymailbox", username);
                                FILE *fp = fopen(filename, "r");
                                if (fp == NULL)
                                {
                                    n = write(connfd, "-ERR\r\n", 6);
                                    if (n < 0) {
                                        perror("Write failed");
                                        exit(EXIT_FAILURE);
                                    }
                                }
                                else
                                {
                                    int mailid = 1;
                                    memset(mail, 0, sizeof(mail));
                                    while (fgets(mail, sizeof(mail), fp))
                                    {
                                        if (strncmp(mail, "From:", 5) == 0)
                                        {
                                            if (mailid == id)
                                            {
                                                n = write(connfd, "+OK\r\n", 5);
                                                if (n < 0) {
                                                    perror("Write failed");
                                                    exit(EXIT_FAILURE);
                                                }
                                                n = write(connfd, mail, strlen(mail));
                                                if (n < 0) {
                                                    perror("Write failed");
                                                    exit(EXIT_FAILURE);
                                                }
                                                #ifdef V
                                                    printf("Server: %s", mail);
                                                #endif
                                                
                                                while (fgets(mail, sizeof(mail), fp))
                                                {
                                                    n = write(connfd, mail, strlen(mail));
                                                    #ifdef V
                                                        printf("Server: %s", mail);
                                                    #endif
                                                    if (n < 0) {
                                                        perror("Write failed");
                                                        exit(EXIT_FAILURE);
                                                    }
                                                    // if \r\n.\r\n is found, break
                                                    if (strncmp(mail, ".\r\n", 3) == 0)
                                                    {
                                                        break;
                                                    }
                                                }
                                                break;
                                            }
                                            mailid++;
                                        }
                                    }
                                    fclose(fp);
                                }
                            }

                            // DELE
                            else if (strncmp(buff, "DELE", 4) == 0)
                            {
                                int id;
                                sscanf(buff, "DELE %d", &id);
                                // append id to delMarked
                                delMarked[id] = 1;
                                
                                n = write(connfd, "+OK\r\n", 5);
                                if (n < 0) {
                                    perror("Write failed");
                                    exit(EXIT_FAILURE);
                                }
                            }

                            // RSET
                            else if (strncmp(buff, "RSET", 4) == 0)
                            {
                                n = write(connfd, "+OK\r\n", 5);
                                if (n < 0) {
                                    perror("Write failed");
                                    exit(EXIT_FAILURE);
                                }
                                memset(delMarked, 0, sizeof(delMarked));
                            }

                            // QUIT
                            else if (strncmp(buff, "QUIT", 4) == 0)
                            {
                                state = 2;
                                break;
                            }

                            // NOOP
                            else if (strncmp(buff, "NOOP", 4) == 0)
                            {
                                n = write(connfd, "+OK\r\n", 5);
                                if (n < 0) {
                                    perror("Write failed");
                                    close(connfd);
                                    exit(EXIT_FAILURE);
                                }
                            }

                            else
                            {
                                n = write(connfd, "-ERR\r\n", 6);
                                if (n < 0) {
                                    perror("Write failed");
                                    close(connfd);
                                    exit(EXIT_FAILURE);
                                }
                                close(connfd);
                                exit(0);
                            }
                        }
                        break;

                    case 2:
                        // Update
                        // check if any mail is marked for deletion
                        int cnt = 0;
                        for (int i = 1; i < 256; i++)
                        {
                            if (delMarked[i] == 1)
                            {
                                cnt++;
                            }
                        }
                        if (cnt != 0)
                        {
                            printf("Server: Deleting %d mails\n", cnt);
                            char filename[272];
                            sprintf(filename, "./%s/mymailbox", username);
                            FILE *fp = fopen(filename, "r");
                            if (fp == NULL)
                            {
                                n = write(connfd, "-ERR\r\n", 6);
                                if (n < 0) {
                                    perror("Write failed");
                                    exit(EXIT_FAILURE);
                                }
                            }
                            else
                            {
                                int mailid = 0;
                                memset(mail, 0, sizeof(mail));
                                FILE *temp = fopen("temp", "w");
                                while (fgets(mail, sizeof(mail), fp))
                                {
                                    if (strncmp(mail, "From:", 5) == 0)
                                    {
                                        mailid++;
                                        if (delMarked[mailid] == 1)
                                        {
                                            #ifdef V
                                                printf("Server: Deleted mail %d\n", mailid);
                                            #endif
                                            continue;
                                        }
                                    }
                                    else
                                    {
                                        if (delMarked[mailid] == 1)
                                        {
                                            continue;
                                        }
                                    }
                                    fprintf(temp, "%s", mail);
                                }
                                fclose(fp);
                                fclose(temp);
                                remove(filename);
                                rename("temp", filename);
                            }
                        }

                        n = write(connfd, "+OK\r\n", 5);
                        if (n < 0) {
                            perror("Write failed");
                            exit(EXIT_FAILURE);
                        }
                        printf("Successfully served client %s:%d\n\n", inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port));
                        close(connfd);
                        exit(0);
                        break;

                    default:
                        break;
                }

            }
            close(connfd);
            exit(0);
        }
    }

    return 0;
}