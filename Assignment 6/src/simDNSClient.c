/*
 *  Author:     Prasanna Paithankar
 *  Roll No.:   21CS30065
 *  Course:     Computer Networks Laboratory (CS39006) Spr 2023-24
 *  Date:       08/04/2024

 *  File:       simDNSClient.c
 *  Compile:    gcc -o simDNSClient simDNSClient.c
 *  Run:        ./simDNSClient -d <destination IP> -i <interface>
 */

#include "../include/simDNS.h"

// ICMP packet structure
struct icmp_packet
{
    struct icmphdr header;
    char data[64 - sizeof(struct icmphdr)];
};


// Compute checksum for ICMP packet
unsigned short
checksum(void *b, int len)
{
    unsigned short *buf = b;
    unsigned int sum = 0;
    unsigned short result;

    for (sum = 0; len > 1; len -= 2)
        sum += *buf++;
    if (len == 1)
        sum += *(unsigned char *)buf;
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    result = ~sum;
    return result;
}


// Send ICMP echo request
int
sendPing(const char *ipAddress)
{
    int sockfd, sent, received;
    struct icmp_packet packet;
    struct sockaddr_in addr;
    struct timeval timeout;
    char recv_buffer[64];

    sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sockfd < 0) {
        perror("Socket creation failed");
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(ipAddress);

    // Fill ICMP packet
    memset(&packet, 0, sizeof(packet));
    packet.header.type = ICMP_ECHO;
    packet.header.code = 0;
    packet.header.un.echo.id = getpid();
    packet.header.un.echo.sequence = 1;
    memset(&packet.data, 'A', sizeof(packet.data));
    packet.header.checksum = checksum(&packet, sizeof(packet));

    // Send ICMP packet
    sent = sendto(sockfd, &packet, sizeof(packet), 0, (struct sockaddr *)&addr, sizeof(addr));
    if (sent < 0) {
        perror("Sendto failed");
        close(sockfd);
        return -1;
    }

    close(sockfd);
    return 0;
}


// Get MAC address from ARP cache
int
getMACAddress(const char *ipAddress, char *macAddress)
{
    // Send ICMP echo request to update ARP cache
    if (sendPing(ipAddress) < 0)
    {
        printf("Failed to send ICMP echo request\n");
        return -1;
    }

    FILE *arpCacheFile;
    char cmd[100], ip[20], hwAddr[20], device[20];
    memset(ip, 0, 20);
    memset(hwAddr, 0, 20);
    memset(device, 0, 20);
    int ret;

    sprintf(cmd, "arp -n | grep %s | awk '{print $1, $3, $5}'", ipAddress);
    arpCacheFile = popen(cmd, "r");
    if (arpCacheFile == NULL)
    {
        perror("Popen failed");
        return -1;
    }

    // Read ARP cache entry
    ret = fscanf(arpCacheFile, "%s %s %s", ip, hwAddr, device);
    if (ret != 3)
    {
        perror("Parsing ARP cache entry failed");
        pclose(arpCacheFile);
        return -1;
    }

    pclose(arpCacheFile);
    memcpy(macAddress, hwAddr, strlen(hwAddr));

    return 0;
}


int
main(int argc, char *argv[])
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

    // Parse arguments
    int opt;
    char *destinationIP;
    int destinationFlag = 0;
    char *interface;
    int interfaceFlag = 0;

    while ((opt = getopt(argc, argv, "d:i:")) != -1)
    {
        switch (opt)
        {
        case 'd':
            destinationIP = optarg;
            destinationFlag = 1;
            break;
        case 'i':
            interface = optarg;
            interfaceFlag = 1;
            break;
        default:
            printf("Usage: %s -d <destination IP> -i <interface>\n", argv[0]);
            return 1;
        }
    }
    
    if (interfaceFlag == 0)
    {
        interface = "lo";
    }

    if (destinationFlag == 0)
    {
        destinationIP = "127.0.0.1";
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
    sll.sll_ifindex = if_nametoindex(interface);
    if (bind(rawSocket, (struct sockaddr *)&sll, sizeof(sll)) < 0)
    {
        perror("Bind failed");
        return 1;
    }

    // Get MAC address of the network interface for source as well as destination
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), interface);
    if (ioctl(rawSocket, SIOCGIFHWADDR, &ifr) < 0)
    {
        perror("Ioctl failed");
        return 1;
    }
    u_int8_t srcmac[6];
    u_int8_t destmac[6];
    memcpy(srcmac, ifr.ifr_hwaddr.sa_data, 6);

    if (interfaceFlag == 0)
    {
        memcpy(destmac, ifr.ifr_hwaddr.sa_data, 6);
    }
    else
    {
        char destMAC[18];
        memset(destMAC, 0, 18);
        if (getMACAddress(destinationIP, destMAC) < 0)
        {
            printf("Failed to get MAC address\n");
            memcpy(destmac, ifr.ifr_hwaddr.sa_data, 6);
        }
        else
        {
            sscanf(destMAC, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &destmac[0], &destmac[1], &destmac[2], &destmac[3], &destmac[4], &destmac[5]);
        }
    }

    // Get IP address of the network interface
    if (ioctl(rawSocket, SIOCGIFADDR, &ifr) < 0)
    {
        perror("Ioctl failed");
        return 1;
    }
    char *sourceIP = inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr);
    if (destinationIP == 0)
    {
        memcpy(destinationIP, sourceIP, strlen(sourceIP));
    }

    fd_set readfds;
    struct timeval timeout;
    int maxfd = rawSocket + 1;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    int activity;

    printf("Enter query string in the format: getIP <numQueries> <domain1> <domain2> ... <domainN>\n");
    printf("Enter EXIT to quit\n");
    printf("------------------------------------------------------------\n");
    printf("$ ");
    fflush(stdout);

    int printFlag = 0;

    while (1)
    {
        if (printFlag == 1)
        {
            printf("$ ");
            fflush(stdout);
            printFlag = 0;
        }

        FD_ZERO(&readfds);
        FD_SET(rawSocket, &readfds);
        FD_SET(STDIN_FILENO, &readfds);

        activity = select(maxfd, &readfds, NULL, NULL, &timeout);

        if (activity < 0)
        {
            perror("Select failed");
            return 1;
        }

        // Check if there is data to read from the raw socket
        if (FD_ISSET(rawSocket, &readfds))
        {
            char buffer[4096];
            memset(buffer, 0, 4096);

            // Receive the response message
            int recbytes = recv(rawSocket, buffer, 4096, 0);
            if (recbytes < 0)
            {
                perror("Receive failed");
                return 1;
            }

            struct ether_header *ethHeader = (struct ether_header *)buffer;
            if (ntohs(ethHeader->ether_type) != ETH_P_IP)   // Check if the packet is an IP packet
            {
                continue;
            }

            struct iphdr *ipHeader = (struct iphdr *)(buffer + sizeof(struct ether_header));
            if (ipHeader->protocol != 254)  // Check if the packet is a simDNS packet
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
                    printf("------------------------------------------------------------\n");
                    printf("Query ID: %d\n", response->id);
                    printf("Total query strings: %d\n\n", response->numResponses);

                    struct SimDNSQuery *queryPacket = (struct SimDNSQuery *)(idPacket[response->id] + sizeof(struct ether_header) + sizeof(struct iphdr));

                    for (i = 0; i < response->numResponses; i++)
                    {
                        if (response->responses[i].valid == 1)
                        {
                            printf("%s :\t%s\n", queryPacket->queries[i].domainName + 4, inet_ntoa(*(struct in_addr *)&response->responses[i].ipAddress));
                        }
                        else
                        {
                            printf("%s :\tNO IP ADDRESS FOUND\n", queryPacket->queries[i].domainName + 4);
                        }
                    }
                    printf("------------------------------------------------------------\n");

                    // Delete the query ID from the pending query table
                    idTable[response->id] = 0;

                    printf("$ ");
                    fflush(stdout);
                }
            }
        }

        // Check if there is data to read from the standard input
        if (FD_ISSET(STDIN_FILENO, &readfds))
        {
            // Get the query string from the user
            fgets(queryStr, 256, stdin);
            if (queryStr[strlen(queryStr) - 1] == '\n')
            {
                queryStr[strlen(queryStr) - 1] = '\0';
            }

            if (strncmp(queryStr, "EXIT", 4) == 0)
            {
                break;
            }

            // Parse the query string
            token = strtok(queryStr, " ");
            if (strcmp(token, "getIP") != 0)
            {
                printf("Invalid query string: Use the format 'getIP <numQueries> <domain1> <domain2> ... <domainN>'\n\n");
                printFlag = 1;
                continue;
            }

            token = strtok(NULL, " ");
            if (token == NULL)
            {
                printf("Enough number of queries not provided\n\n");
                printFlag = 1;
                continue;
            }
            numQueries = atoi(token);
            if (numQueries > 8)
            {
                printf("Number of queries should be less than or equal to 8\n\n");
                printFlag = 1;
                continue;
            }

            char **domains = (char **)malloc(numQueries * sizeof(char *));
            for (i = 0; i < numQueries; i++)
            {
                domains[i] = (char *)malloc(28 * sizeof(char));
                memset(domains[i], 0, 28);
            }

            for (i = 0; i < numQueries; i++)
            {
                token = strtok(NULL, " ");
                if (token == NULL)
                {
                    printf("Domain name not provided\n\n");
                    break;
                }
                if (strlen(token) < 3 || strlen(token) > 31)
                {
                    printf("Domain name should be between 3 and 31 characters\n\n");
                    break;
                }
                if (token[0] == '-' || token[strlen(token) - 1] == '-')
                {
                    printf("Hyphen cannot be used at the beginning or end of a domain name\n\n");
                    break;
                }
                if (strstr(token, "--") != NULL)
                {
                    printf("Two consecutive hyphens are not allowed\n\n");
                    break;
                }
                if (strspn(token, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-.") != strlen(token))
                {
                    printf("Only alphanumeric characters and hyphens are allowed\n\n");
                    break;
                }

                strcpy(domains[i], token);
            }

            if (i < numQueries)
            {
                printFlag = 1;
                continue;
            }

            timeout.tv_sec = 5;
            timeout.tv_usec = 0;

            // Construct the query message
            memset(&query, 0, sizeof(struct SimDNSQuery));
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

            // Create Ethernet header
            struct ether_header *eth = (struct ether_header *)malloc(sizeof(struct ether_header));
            eth->ether_type = htons(ETH_P_IP);
            memcpy(eth->ether_shost, srcmac, 6);
            memcpy(eth->ether_dhost, destmac, 6);
            
            // Create IP header
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
            ip->saddr = inet_addr(sourceIP);
            ip->daddr = inet_addr(destinationIP);

            // Create packet
            memset(idPacket[id], 0, idPacketSize);
            memcpy(idPacket[id], eth, sizeof(struct ether_header));
            memcpy(idPacket[id] + sizeof(struct ether_header), ip, sizeof(struct iphdr));
            memcpy(idPacket[id] + sizeof(struct ether_header) + sizeof(struct iphdr), &query, sizeof(struct SimDNSQuery));

            // Send the packet
            if (send(rawSocket, idPacket[id], sizeof(struct ether_header) + sizeof(struct iphdr) + sizeof(struct SimDNSQuery), 0) < 0)
            {
                printf("%d", errno);
                perror("Send failed");
                exit(1);
            }
        
            // Insert the query ID into the pending query table
            idTable[id] = 1;
            id = (id + 1) % 1024;

            // Garbage collection
            free(eth);
            free(ip);
            for (i = 0; i < numQueries; i++)
            {
                free(domains[i]);
            }
            free(domains);
        }

        // Retransmit queries with no response
        if (activity == 0)
        {
            timeout.tv_sec = 5;
            timeout.tv_usec = 0;

            for (i = 0; i < 1024; i++)
            {
                if (idTable[i] > 0)
                {
                    // If the query has been retransmitted 3 times, print an error message
                    if (idTable[i] == 4)
                    {
                        printf("ERROR: NO RESPONSE on query ID: %d\n", i);
                        idTable[i] = 0;
                    }
                    else
                    {
                        // Retransmit the query
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

    // Garbage collection
    close(rawSocket);

    for (i = 0; i < 1024; i++)
    {
        free(idPacket[i]);
    }
    free(idPacket);

    return 0;
}
