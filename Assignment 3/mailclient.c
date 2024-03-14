/*
 *  Author:     Prasanna Paithankar
 *  Roll No.:   21CS30065
 *  Course:     Computer Networks Laboratory (CS39006) Spr 2023-24
 *  Date:       01/02/2024

 *  File:       smtpclient.c
 *  Compile:    gcc -o smtpclient smtpclient.c [-DV for verbose]
 *  Run:        ./smtpclient [server_IP] [smtp_port] [pop3_port]
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
#include <sys/wait.h>
#include <termios.h>

// Signal handler for SIGINT
void sigintHandler(int sig_num)
{
    write(1, "\nForced quit\n\nAuthor: Prasanna Paithankar (21CS30065)\n", 55);
    exit(0);
}

// Function to get password without echoing it
int getch() 
{
    int ch;
    struct termios old_settings, new_settings;
    tcgetattr(STDIN_FILENO, &old_settings);
    new_settings = old_settings;
    
    new_settings.c_lflag &= ~(ICANON | ECHO);   // change the settings for by disabling ECHO mode
    tcsetattr(STDIN_FILENO, TCSANOW, &new_settings);
    ch = getchar();
    
    tcsetattr(STDIN_FILENO, TCSANOW, &old_settings);    // reset back to default settings
    return ch;
}

int main(int argc, char *argv[])
{
    int len, i = 0;
    char ch;
    struct sockaddr_in servaddr, cliaddr;
    char buff[4096];
    char from[256], to[256];
    char mail[4096], subject[80];
    char username[256], password[256];

    // Handle SIGINT
    signal(SIGINT, sigintHandler);

    printf("Welcome to SMTP Client\n\n");
    printf("Enter username: ");
    scanf("%s", username);
    if (username[strlen(username) - 1] == '\n')
        username[strlen(username) - 1] = '\0';
    getchar();
    
    printf("Enter password: ");
    while ((ch = getch()) != '\n') 
    {
        if (ch == 127 || ch == 8) 
        {
            if (i != 0) {
                i--;
                printf("\b \b");
            }
        } 
        else 
        {
            password[i++] = ch;
            // echo the '*' for every character
            printf("*");
        }
    }
    password[i] = '\0'; // Null-terminate the password string
    printf("\n\n");

    while (1) 
    {
        printf("1. Manage Mail\n2. Send Mail\n3. Quit\n");
        int n = 1;
        scanf("%d", &n);
        printf("\n");

        // Manage mail
        if (n == 1) 
        {
            int connfd = socket(AF_INET, SOCK_STREAM, 0);
            if (connfd < 0) 
            {
                perror("Socket creation failed");
                exit(EXIT_FAILURE);
            }

            // Set socket options
            int opt = 1;
            if (setsockopt(connfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) 
            {
                perror("Setsockopt failed");
                exit(EXIT_FAILURE);
            }

            // Bind socket to port
            servaddr.sin_family = AF_INET;
            inet_pton(AF_INET, argv[1], &servaddr.sin_addr);
            if (argc > 2)
                servaddr.sin_port = htons(atoi(argv[3]));
            else
                servaddr.sin_port = htons(25);

            if (connect(connfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) 
            {
                perror("Connect failed");
                exit(EXIT_FAILURE);
            }

            int flag = 0;

            // Recieve greeting from pop3 server
            memset(buff, 0, sizeof(buff));
            while (1) 
            {
                int n = read(connfd, buff, 64);
                if (strstr(buff, "+OK") != NULL)
                {
                    break;
                }
                else if (strstr(buff, "-ERR") != NULL)
                {
                    printf("Greeting not recognized\n\n");
                    flag = 1;
                    break;
                }
            }
            #ifdef V
                printf("%s", buff);
            #endif
            
            // Send username
            memset(buff, 0, sizeof(buff));
            sprintf(buff, "USER %s\r\n", username);
            write(connfd, buff, strlen(buff));
            memset(buff, 0, sizeof(buff));
            while (1) 
            {
                int n = read(connfd, buff, 64);
                if (strstr(buff, "+OK") != NULL)
                {
                    break;
                }
                else if (strstr(buff, "-ERR") != NULL)
                {
                    printf("Invalid username\n\n");
                    flag = 1;
                    break;
                }
            }

            if (flag)
            {
                write(connfd, "QUIT\r\n", 6);
                close(connfd);
                continue;
            }

            // Send password
            memset(buff, 0, sizeof(buff));
            sprintf(buff, "PASS %s\r\n", password);
            write(connfd, buff, strlen(buff));
            memset(buff, 0, sizeof(buff));
            while (1) 
            {
                int n = read(connfd, buff, 64);
                if (strstr(buff, "+OK") != NULL)
                {
                    break;
                }
                else if (strstr(buff, "-ERR") != NULL)
                {
                    printf("Invalid password\n\n");
                    flag = 1;
                    break;
                }
            }

            if (flag)
            {
                write(connfd, "QUIT\r\n", 6);
                close(connfd);
                continue;
            }

            // Send STAT
            write(connfd, "STAT\r\n", 6);
            memset(buff, 0, sizeof(buff));
            while (1) 
            {
                int n = read(connfd, buff, 64);
                // break only after recieveing whole line till CRLF
                if (strstr(buff, "\r\n") != NULL)
                {
                    break;
                }
            }
            if (strstr(buff, "-ERR") != NULL)
            {
                printf("STAT not recognized\n\n");
                write(connfd, "QUIT\r\n", 6);
                close(connfd);
                continue;
            }

            // Get number of mails
            int num = 0;
            sscanf(buff, "+OK %d", &num);
            printf("Number of mails: %d\n", num);

            // Send LIST
            write(connfd, "LIST\r\n", 6);
            memset(buff, 0, sizeof(buff));
            while (1) 
            {
                int n = read(connfd, buff, 64);
                // break only after recieveing whole line till CRLF
                if (strstr(buff, "\r\n") != NULL)
                {
                    break;
                }
            }
            if (strstr(buff, "-ERR") != NULL)
            {
                printf("LIST not recognized\n\n");
                write(connfd, "QUIT\r\n", 6);
                close(connfd);
                continue;
            }

            // Get mail list
            char mailList[4096];
            memset(mailList, 0, sizeof(mailList));
            memset(buff, 0, sizeof(buff));
            printf("\nMail list:\n");
            while (1) 
            {
                int n = read(connfd, buff, 64);
                // append to mailist
                strcat(mailList, buff);
                // break only after recieveing \r\n.\r\n
                if (strstr(mailList, "\r\n.\r\n") != NULL)
                {
                    break;
                }
                if (strstr(mailList, "-ERR") != NULL)
                {
                    printf("LIST not recognized\n\n");
                    write(connfd, "QUIT\r\n", 6);
                    flag = 1;
                    close(connfd);
                }
                memset(buff, 0, sizeof(buff));
            }
            if (flag)
            {
                continue;
            }
            // print mail list 
            printf("| No. | From | Time | Subject |\n");
            printf(" ----- ------ ------ ---------\n");
            // print mail list until strlen(mailList) - 5 to remove \r\n.\r\n
            fprintf(stdout, "%.*s", (int)strlen(mailList) - 5, mailList);
            printf("\n\n");

            int del = 0;
            while (1)
            {
                // Send RETR
                printf("Enter mail number to read (-1 to go back): ");
                int mailNum;
                scanf("%d", &mailNum);
                if (mailNum == -1)
                {
                    if (del)
                    {
                        printf("Are you sure you want to delete the mails marked for deletion? (y/n): ");
                        getchar();
                        char in = getchar();
                        printf("\f");
                        if (in == 'y')
                        {
                            break;
                        }
                        else
                        {
                            write(connfd, "RSET\r\n", 6);
                            memset(buff, 0, sizeof(buff));
                            while (read(connfd, buff, 64))
                            {
                                if (strstr(buff, "+OK") != NULL)
                                {
                                    break;
                                }
                                else if (strstr(buff, "-ERR") != NULL)
                                {
                                    printf("RSET not recognized\n\n");
                                    flag = 1;
                                    break;
                                }
                            }
                            break;
                        }
                    }
                    break;
                }
                if (mailNum < 1 || mailNum > num)
                {
                    printf("Invalid mail number\n\n");
                    continue;
                }
                printf("\n");
                memset(buff, 0, sizeof(buff));
                sprintf(buff, "RETR %d\r\n", mailNum);
                write(connfd, buff, strlen(buff));
                memset(buff, 0, sizeof(buff));
                memset(mail, 0, sizeof(mail));
                while (1) 
                {
                    int n = read(connfd, buff, 64);
                    // append to mail
                    strcat(mail, buff);
                    if (strstr(buff, "-ERR") != NULL)
                    {
                        printf("RETR not recognized\n\n");
                        flag = 1;
                        break;
                    }
                    // break only after recieveing whole mail
                    if (strstr(mail, "\r\n.\r\n") != NULL)
                    {
                        break;
                    }
                    memset(buff, 0, sizeof(buff));
                }

                if (flag)
                {
                    write(connfd, "QUIT\r\n", 6);
                    close(connfd);
                    continue;
                }
                // Skip the first line
                char *ptr = strtok(mail, "\r\n");
                ptr = strtok(NULL, "\r\n");
                printf("Mail:\n");
                while (ptr != NULL)
                {
                    printf("%s\n", ptr);
                    ptr = strtok(NULL, "\r\n");
                }
                printf("\n");

                // Ask for deletion
                printf("Enter d to delete mail, any other key to continue: ");
                getchar();
                char in = getchar();
                if (in == 'd')
                {
                    del = 1;
                    printf("\nMail marked for deletion\n");
                    // Send DELE
                    memset(buff, 0, sizeof(buff));
                    sprintf(buff, "DELE %d\r\n", mailNum);
                    #ifdef V
                        printf("DELE %d\n", mailNum);
                    #endif
                    write(connfd, buff, strlen(buff));
                    memset(buff, 0, sizeof(buff));
                    while (1) 
                    {
                        int n = read(connfd, buff, 64);
                        if (strstr(buff, "+OK") != NULL)
                        {
                            break;
                        }
                        else if (strstr(buff, "-ERR") != NULL)
                        {
                            printf("DELE not recognized\n\n");
                            flag = 1;
                            break;
                        }
                    }
                }

                if (flag)
                {
                    write(connfd, "QUIT\r\n", 6);
                    close(connfd);
                    continue;
                }
                getchar();
            }

            // Send QUIT
            write(connfd, "QUIT\r\n", 6);
            memset(buff, 0, sizeof(buff));
            while (1) 
            {
                int n = read(connfd, buff, 64);
                if (strstr(buff, "+OK") != NULL)
                {
                    break;
                }
                else if (strstr(buff, "-ERR") != NULL)
                {
                    printf("QUIT not recognized\n\n");
                    flag = 1;
                    break;
                }
            }

            printf("\n");
            close(connfd);
        }

        // Send mail
        if (n == 2) 
        {
            // Create socket
            int connfd = socket(AF_INET, SOCK_STREAM, 0);
            if (connfd < 0) 
            {
                perror("Socket creation failed");
                exit(EXIT_FAILURE);
            }

            // Set socket options
            int opt = 1;
            if (setsockopt(connfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) 
            {
                perror("Setsockopt failed");
                exit(EXIT_FAILURE);
            }

            // Bind socket to port
            servaddr.sin_family = AF_INET;
            servaddr.sin_addr.s_addr = INADDR_ANY;
            if (argc > 2)
                servaddr.sin_port = htons(atoi(argv[2]));
            else
                servaddr.sin_port = htons(25);

            if (connect(connfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) 
            {
                perror("Connect failed");
                exit(EXIT_FAILURE);
            }

            int flag = 0;

            memset(from, 0, sizeof(from));
            memset(to, 0, sizeof(to));
            memset(subject, 0, sizeof(subject));
            memset(mail, 0, sizeof(mail));
            memset(buff, 0, sizeof(buff));

            // Get sender and receiver
            printf("From: ");
            scanf("%s", from);
            if (from[strlen(from) - 1] == '\n')
                from[strlen(from) - 1] = '\0';
            if (strchr(from, '@') == NULL) 
            {
                fprintf(stderr, "Invalid email address\n");
                write(connfd, "QUIT\r\n", 6);
                continue;
            }
            // check if the username is same as the sender part before @
            char ptr[256];
            strcpy(ptr, from);
            strtok(ptr, "@");
            if (strcmp(ptr, username) != 0)
            {
                fprintf(stderr, "Invalid user, use your own email address\n\n");
                write(connfd, "QUIT\r\n", 6);
                continue;
            }
            getchar();

            printf("To: ");
            scanf("%s", to);
            if (strchr(to, '@') == NULL) 
            {
                fprintf(stderr, "Invalid email address\n");
                write(connfd, "QUIT\r\n", 6);
                continue;
            }
            getchar();
            if (to[strlen(to) - 1] == '\n')
                to[strlen(to) - 1] = '\0';
            
            
            printf("Subject: ");
            scanf("%[^\n]s", subject);
            if (subject[strlen(subject) - 1] == '\n')
                subject[strlen(subject) - 1] = '\0';
            getchar();

            // Get mail body
            printf("Enter mail body (end with a single dot): \n");
            mail[0] = '\0';
            while (1) 
            {
                fgets(buff, 4096, stdin);
                if (strcmp(buff, ".\n") == 0)
                    break;
                strcat(mail, buff);
            }
            if (mail[strlen(mail) - 1] == '\n')
                mail[strlen(mail) - 1] = '\0';

            // Send HELO
            write(connfd, "HELO smtp.pipi.ac.in\r\n", 22);

            while (n = read(connfd, buff, 64))
            {
                if (strstr(buff, "250") != NULL)
                {
                    break;
                }
                else if (strstr(buff, "500") != NULL)
                {
                    printf("HELO not recognized\n\n");
                    flag = 1;
                    break;
                }
            }

            if (flag)
            {
                write(connfd, "QUIT\r\n", 6);
                close(connfd);
                continue;
            }

            #ifdef V
                printf("HELO sent and 250 received\n");
            #endif

            // Send MAIL FROM
            sprintf(buff, "MAIL FROM:<%s>\r\n", from);
            write(connfd, buff, strlen(buff));

            while (n = read(connfd, buff, 64))
            {
                if (strstr(buff, "250") != NULL)
                {
                    break;
                }
                else if (strstr(buff, "500") != NULL)
                {
                    printf("MAIL FROM not recognized\n\n");
                    flag = 1;
                    break;
                }
                else if (strstr(buff, "550") != NULL)
                {
                    printf("No such user\n\n");
                    flag = 1;
                    break;
                }
            }

            if (flag)
            {
                write(connfd, "QUIT\r\n", 6);
                close(connfd);
                continue;
            }

            #ifdef V
                printf("MAIL FROM sent and 250 received\n");
            #endif

            // Send RCPT TO
            sprintf(buff, "RCPT TO:<%s>\r\n", to);
            write(connfd, buff, strlen(buff));

            while (n = read(connfd, buff, 64))
            {
                if (strstr(buff, "250") != NULL)
                {
                    break;
                }
                else if (strstr(buff, "500") != NULL)
                {
                    printf("RCPT TO not recognized\n\n");
                    flag = 1;
                    break;
                }
                else if (strstr(buff, "550") != NULL)
                {
                    printf("No such recipient\n\n");
                    flag = 1;
                    break;
                }
            }

            if (flag)
            {
                write(connfd, "QUIT\r\n", 6);
                close(connfd);
                continue;
            }

            #ifdef V
                printf("RCPT TO sent and 250 received\n");
            #endif

            // Send DATA
            write(connfd, "DATA\r\n", 6);

            while (n = read(connfd, buff, 64))
            {
                if (strstr(buff, "354") != NULL)
                {
                    break;
                }
                else if (strstr(buff, "500") != NULL)
                {
                    printf("DATA not recognized\n\n");
                    flag = 1;
                    break;
                }
            }

            if (flag)
            {
                write(connfd, "QUIT\r\n", 6);
                close(connfd);
                continue;
            }

            #ifdef V
                printf("DATA sent and 354 received\n");
            #endif

            // Send mail
            memset(buff, 0, sizeof(buff));
            sprintf(buff, "From: %s\n", from);
            write(connfd, buff, strlen(buff));

            memset(buff, 0, sizeof(buff));
            sprintf(buff, "To: %s\n", to);
            write(connfd, buff, strlen(buff));

            memset(buff, 0, sizeof(buff));
            sprintf(buff, "Subject: %s\n", subject);
            write(connfd, buff, strlen(buff));
            
            write(connfd, mail, strlen(mail));
            write(connfd, "\r\n.\r\n", 5);

            while (n = read(connfd, buff, 64))
            {
                if (strstr(buff, "250") != NULL)
                {
                    break;
                }
                else if (strstr(buff, "500") != NULL)
                {
                    printf("DATA not recognized\n\n");
                    flag = 1;
                    break;
                }
            }

            if (flag)
            {
                write(connfd, "QUIT\r\n", 6);
                close(connfd);
                continue;
            }

            #ifdef V
                printf("Mail sent and 250 received\n");
            #endif

            // send QUIT
            write(connfd, "QUIT\r\n", 6);

            while (n = read(connfd, buff, 64))
            {
                if (strstr(buff, "221") != NULL)
                {
                    break;
                }
                else if (strstr(buff, "500") != NULL)
                {
                    printf("QUIT not recognized\n\n");
                    flag = 1;
                    break;
                }
            }

            #ifdef V
                printf("QUIT sent and 221 received\n");
            #endif

            printf("Mail sent successfully\n\n\f");
            close(connfd);
        }

        // Quit
        else if (n == 3) 
        {
            printf("Quitting\n\nAuthor: Prasanna Paithankar (21CS30065)\n");
            break;
        }
    }

    return 0;
}



    