#include "../include/simDNS.h"

int main()
{
    int rawSocket;
    struct SimDNSQuery query;
    char queryStr[256];
    char *token;
    int i, numQueries;
    int idTable[1024];
    memset(idTable, 0, 1024 * sizeof(int));
    int id = 0;
    
    char **idPacket;
    int idPacketSize = sizeof(struct SimDNSQuery) + sizeof(struct ether_header) + sizeof(struct iphdr);
    
    idPacket = (char **)malloc(1024 * sizeof(char *));
    for (i = 0; i < 1024; i++)
    {
        idPacket[i] = (char *)malloc(idPacketSize);
        memset(idPacket[i], 0, idPacketSize);
    }

    // Create a raw socket
    rawSocket = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (rawSocket < 0)
    {
        perror("Socket creation failed");
        return 1;
    }

    // Bind the raw socket to the network interface
    struct sockaddr_ll sll;
    memset(&sll, 0, sizeof(sll));
    sll.sll_family = AF_PACKET;
    sll.sll_protocol = htons(ETH_P_ALL);
    sll.sll_ifindex = if_nametoindex("lo");
    if (bind(rawSocket, (struct sockaddr *)&sll, sizeof(sll)) < 0)
    {
        perror("Bind failed");
        return 1;
    }

    fd_set readfds;
    struct timeval timeout;
    int maxfd = rawSocket + 1;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    int activity;

    int printFlag = 1;

    while (1)
    {
        FD_ZERO(&readfds);
        FD_SET(rawSocket, &readfds);
        FD_SET(STDIN_FILENO, &readfds);

        if (printFlag == 1)
        {
            printf("Enter the query string: ");
            fflush(stdout);
            printFlag = 0;
        }

        activity = select(maxfd, &readfds, NULL, NULL, &timeout);

        if (activity < 0)
        {
            perror("Select failed");
            return 1;
        }

        if (FD_ISSET(rawSocket, &readfds))
        {
            char buffer[4096];
            memset(buffer, 0, 4096);

            // Receive the response message
            int recbytes = recv(rawSocket, buffer, 4096, 0);
            
            struct ether_header *ethHeader = (struct ether_header *)buffer;
            if (ntohs(ethHeader->ether_type) != ETH_P_IP)
            {
                continue;
            }

            struct iphdr *ipHeader = (struct iphdr *)(buffer + sizeof(struct ether_header));
            if (ipHeader->protocol != 254)
            {
                continue;
            }

            timeout.tv_sec = 5;
            timeout.tv_usec = 0;

            struct SimDNSResponse *response = (struct SimDNSResponse *)(buffer + sizeof(struct ether_header) + sizeof(struct iphdr));


            // Check if the response is valid
            if (response->messageType == 1)
            {
                // Check if the query ID is valid
                if (idTable[response->id] == 1)
                {
                    // Print the response
                    printf("Query ID: %d\n", response->id);
                    printf("Total query strings: %d\n", response->numResponses);

                    struct SimDNSQuery *queryPacket = (struct SimDNSQuery *)(idPacket[response->id] + sizeof(struct ether_header) + sizeof(struct iphdr));

                    for (i = 0; i < response->numResponses; i++)
                    {
                        if (response->responses[i].valid == 1)
                        {
                            printf("%s %s\n", queryPacket->queries[i].domainName + 4, inet_ntoa(*(struct in_addr *)&response->responses[i].ipAddress));
                        }
                        else
                        {
                            printf("%s NO IP ADDRESS FOUND\n", queryPacket->queries[i].domainName + 4);
                        }
                    }

                    // Delete the query ID from the pending query table
                    idTable[response->id] = 0;
                }
            }
        }

        if (FD_ISSET(STDIN_FILENO, &readfds))
        {
            // Get the query string from the user
            fgets(queryStr, 256, stdin);
            if (queryStr[strlen(queryStr) - 1] == '\n')
            {
                queryStr[strlen(queryStr) - 1] = '\0';
            }

            printFlag = 1;

            if (strncmp(queryStr, "EXIT", 4) == 0)
            {
                break;
            }

            // Parse the query string
            token = strtok(queryStr, " ");
            if (strcmp(token, "getIP") != 0)
            {
                printf("Invalid query string\n");
                return 1;
            }

            token = strtok(NULL, " ");
            numQueries = atoi(token);
            if (numQueries > 8)
            {
                printf("Number of queries should be less than or equal to 8\n");
                continue;
            }

            // Array of string to store the domains with dimensions numQueries x 28 which will be malloced later
            char **domains = (char **)malloc(numQueries * sizeof(char *));
            for (i = 0; i < numQueries; i++)
            {
                domains[i] = (char *)malloc(28 * sizeof(char));
                memset(domains[i], 0, 28);
            }

            for (i = 0; i < numQueries; i++)
            {
                token = strtok(NULL, " ");
                if (strlen(token) < 3 || strlen(token) > 31)
                {
                    printf("Domain name should be between 3 and 31 characters\n");
                    break;
                }
                if (token[0] == '-' || token[strlen(token) - 1] == '-')
                {
                    printf("Hyphen cannot be used at the beginning or end of a domain name\n");
                    break;
                }
                if (strstr(token, "--") != NULL)
                {
                    printf("Two consecutive hyphens are not allowed\n");
                    break;
                }
                // if (strspn(token, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-.") != strlen(token))
                // {
                //     printf("Only alphanumeric characters and hyphens are allowed\n");
                //     break;
                // }

                strcpy(domains[i], token);
            }

            if (i < numQueries)
            {
                continue;
            }

            timeout.tv_sec = 5;
            timeout.tv_usec = 0;

            // Construct the query message
            query.id = id;
            query.messageType = 0;
            query.numQueries = numQueries;
            for (i = 0; i < numQueries; i++)
            {
                char sizeOfDomainName[4];
                int size = strlen(domains[i]);

                if (size < 10)
                {
                    sprintf(sizeOfDomainName, "000%d", size);
                }
                else if (size < 100)
                {
                    sprintf(sizeOfDomainName, "00%d", size);
                }
                else if (size < 1000)
                {
                    sprintf(sizeOfDomainName, "0%d", size);
                }
                else
                {
                    sprintf(sizeOfDomainName, "%d", size);
                }
                memcpy(query.queries[i].domainName, sizeOfDomainName, 4);
                memcpy(query.queries[i].domainName + 4, domains[i], size);
            }

            // Send the query message
            // create ethernet header
            struct ether_header *eth = (struct ether_header *)malloc(sizeof(struct ether_header));
            eth->ether_type = htons(ETH_P_IP);
            sscanf(SRC_MAC, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &eth->ether_shost[0], &eth->ether_shost[1], &eth->ether_shost[2], &eth->ether_shost[3], &eth->ether_shost[4], &eth->ether_shost[5]);
            sscanf(DST_MAC, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &eth->ether_dhost[0], &eth->ether_dhost[1], &eth->ether_dhost[2], &eth->ether_dhost[3], &eth->ether_dhost[4], &eth->ether_dhost[5]);
            
            // create IP header
            struct iphdr *ip = (struct iphdr *)malloc(sizeof(struct iphdr));
            ip->version = 4;
            ip->ihl = 5;
            ip->tos = 0;
            ip->tot_len = htons(sizeof(struct iphdr) + sizeof(struct SimDNSQuery));
            ip->id = htons(0);
            ip->frag_off = 0;
            ip->ttl = 64;
            ip->protocol = 254;
            ip->check = 0;
            ip->saddr = inet_addr("127.0.0.1");
            ip->daddr = inet_addr("127.0.0.1");

            // create packet
            memset(idPacket[id], 0, idPacketSize);
            memcpy(idPacket[id], eth, sizeof(struct ether_header));
            memcpy(idPacket[id] + sizeof(struct ether_header), ip, sizeof(struct iphdr));
            memcpy(idPacket[id] + sizeof(struct ether_header) + sizeof(struct iphdr), &query, sizeof(struct SimDNSQuery));

            // send the packet
            if (send(rawSocket, idPacket[id], sizeof(struct ether_header) + sizeof(struct iphdr) + sizeof(struct SimDNSQuery), 0) < 0)
            {
                printf("%d", errno);
                perror("Send failed");
                exit(1);
            }
        
            // Insert the query ID into the pending query table
            idTable[id] = 1;
            // idPacket[id] = packet;
            id = (id + 1) % 1024;

            free(eth);
            free(ip);
            for (i = 0; i < numQueries; i++)
            {
                free(domains[i]);
            }
            free(domains);
        }

        if (activity == 0)
        {
            timeout.tv_sec = 5;
            timeout.tv_usec = 0;

            for (i = 0; i < 1024; i++)
            {
                if (idTable[i] > 0)
                {
                    if (idTable[i] == 4)
                    {
                        printf("ERROR: NO RESPONSE on query ID: %d\n", i);
                        idTable[i] = 0;
                    }
                    else
                    {
                        // struct SimDNSQuery *simfun = (struct SimDNSQuery *)(idPacket[i] + sizeof(struct ether_header) + sizeof(struct iphdr));
                        // printf("Retransmitting query ID: %d with domains %s\n", i, simfun->queries[1].domainName + 4);
                        if (send(rawSocket, idPacket[i], sizeof(struct ether_header) + sizeof(struct iphdr) + sizeof(struct SimDNSQuery), 0) < 0)
                        {
                            perror("Send failed");
                            exit(1);
                        }
                        idTable[i]++;
                    }
                }
            }
        }
    }

    // Close the raw socket
    close(rawSocket);

    for (i = 0; i < 1024; i++)
    {
        free(idPacket[i]);
    }
    free(idPacket);

    return 0;
}
