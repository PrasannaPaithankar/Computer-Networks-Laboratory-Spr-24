/*
 *  Author:     Prasanna Paithankar
 *  Roll No.:   21CS30065
 *  Course:     Computer Networks Laboratory (CS39006) Spr 2023-24
 *  Date:       08/04/2024

 *  File:       simDNSServer.c
 *  Compile:    gcc -o simDNSServer simDNSServer.c
 *  Run:        ./simDNSServer -i <interface>
 */

#include "../include/simDNS.h"

FILE *fp;

void
signalHandler(int signum)
{
    if (signum == SIGINT)
    {
        write(1, "\nsimDNSServer shutting down...\n", 31);
        write(1, "\nAuthor: Prasanna Paithankar\n", 29);
        exit(0);
    }
}


// Function to drop a message with a given probability
int
dropmessage(float prob)
{
    return (rand() < prob * RAND_MAX);
}


// Function to process the query and generate the response
void
processQuery(struct SimDNSQuery *query, struct SimDNSResponse *response)
{
    struct hostent *host;
    int i;

    // Initialize the response
    response->id = query->id;
    response->messageType = 1;
    response->numResponses = 0;

    // For each query, get the IP address
    for (i = 0; i < query->numQueries; i++) {
        char sizeOfDomainName[4];
        char domainName[32];
        int j;

        // Get the size of the domain name
        memcpy(sizeOfDomainName, query->queries[i].domainName, 4);
        sizeOfDomainName[4] = '\0';
        int size = atoi(sizeOfDomainName);

        // Get the domain name
        memcpy(domainName, query->queries[i].domainName + 4, size);
        domainName[size] = '\0';

        // Get the IP address
        host = gethostbyname(domainName);

        if (host == NULL)
        {
            fprintf(fp, "Could not resolve %s\n", domainName);
            response->responses[i].valid = 0;
            response->responses[i].ipAddress = 0;
        }
        else
        {
            fprintf(fp, "Resolved %s to %s\n", domainName, inet_ntoa(*(struct in_addr *)host->h_addr_list[0]));
            response->responses[i].valid = 1;
            response->responses[i].ipAddress = ((struct in_addr *)host->h_addr_list[0])->s_addr;
        }

        response->numResponses++;
    }

    fflush(fp);
    return;
}


int
main(int argc, char *argv[])
{
    // Client cache
    struct ClientCache clientCache[100];
    int numClients = 0;

    // Open the log file
    fp = fopen("logs/simDNSServer.log", "a");
    if (fp == NULL)
    {
        perror("fopen() failed");
        exit(1);
    }
    fprintf(fp, "simDNSServer started\n");

    // Register the signal handler
    signal(SIGINT, signalHandler);

    // Parse arguments
    int opt;
    char *interface;
    int interfaceFlag = 0;

    while ((opt = getopt(argc, argv, "i:")) != -1)
    {
        switch (opt)
        {
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
    
    // Create a raw socket
    int rawSocket = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (rawSocket < 0) {
        perror("Socket creation failed");
        exit(1);
    }

    // Bind the raw socket to the local IP address
    struct sockaddr_ll sll;
    memset(&sll, 0, sizeof(sll));
    sll.sll_family = AF_PACKET;
    sll.sll_protocol = htons(ETH_P_ALL);
    sll.sll_ifindex = if_nametoindex(interface);
    inet_pton(AF_INET, "127.0.0.1", sll.sll_addr);

    if (bind(rawSocket, (struct sockaddr *)&sll, sizeof(sll)) == -1) {
        perror("bind() failed");
        close(rawSocket);
        exit(1);
    }

    printf("simDNSServer started...\n");
    printf("Interface: %s\n", interface);
    printf("Server log: logs/simDNSServer.log\n");
    printf("Press Ctrl+C to shutdown\n");

    // Read all the packets that are received to this socket
    while (1)
    {
        fflush(fp);

        struct ether_header *ethHeader;
        struct iphdr *ipHeader;
        struct SimDNSQuery *simDNSQuery;
        struct SimDNSResponse response;

        // Initialize the buffer
        char buffer[65536];
        memset(buffer, 0, sizeof(buffer));
        int bytesRead;

        bytesRead = recv(rawSocket, buffer, sizeof(buffer), 0);
        if (bytesRead < 0) {
            perror("Receive failed");
            exit(1);
        }

        // Check the IP header to see if the protocol field is 254
        ethHeader = (struct ether_header *)buffer;
        if (ntohs(ethHeader->ether_type) != ETH_P_IP)
        {
            continue;
        }

        ipHeader = (struct iphdr *)(buffer + sizeof(struct ether_header));
        if (ipHeader->protocol != 254)
        {
            continue;
        }
        
        // If the protocol field is 254, then read the query header
        simDNSQuery = (struct SimDNSQuery *)(buffer + sizeof(struct ether_header) + sizeof(struct iphdr));

        // If it is a simDNS query, then parse the query strings
        if (simDNSQuery->messageType == 0)
        {
            if (dropmessage(P))
            {
                fprintf(fp, "Dropped query from client %s\n", inet_ntoa(*(struct in_addr *)&ipHeader->saddr));
                continue;
            }

            // Check if the client is already in the cache
            int i;
            int clientFound = 0;
            for (i = 0; i < numClients; i++)
            {
                if (clientCache[i].ipAddress == ipHeader->saddr)
                {
                    clientFound = 1;
                    break;
                }
            }

            if (clientFound == 1)
            {
                if (clientCache[i].id == simDNSQuery->id)
                {
                    fprintf(fp, "Caught duplicate\n");
                    continue;
                }
                else
                {
                    clientCache[i].id = simDNSQuery->id;
                }
            }
            else
            {
                clientCache[numClients].ipAddress = ipHeader->saddr;
                clientCache[numClients].id = simDNSQuery->id;
                numClients++;
            }

            fprintf(fp, "Received query from client %s\n", inet_ntoa(*(struct in_addr *)&ipHeader->saddr));

            // Generate a simDNS response
            processQuery(simDNSQuery, &response);

            // Create the Ethernet header
            struct ether_header ethHeaderResponse;
            memcpy(ethHeaderResponse.ether_dhost, ethHeader->ether_shost, 6);
            memcpy(ethHeaderResponse.ether_shost, ethHeader->ether_dhost, 6);
            ethHeaderResponse.ether_type = htons(ETH_P_IP);

            // Create the IP header
            struct iphdr ipHeaderResponse;
            ipHeaderResponse.version = 4;
            ipHeaderResponse.ihl = 5;
            ipHeaderResponse.tos = 0;
            ipHeaderResponse.tot_len = htons(sizeof(struct iphdr) + sizeof(struct SimDNSResponse));
            ipHeaderResponse.id = htons(0);
            ipHeaderResponse.frag_off = 0;
            ipHeaderResponse.ttl = 64;
            ipHeaderResponse.protocol = 254;
            ipHeaderResponse.check = 0;
            ipHeaderResponse.saddr = ipHeader->daddr;
            ipHeaderResponse.daddr = ipHeader->saddr;

            // Create the simDNS response
            char responseBuffer[sizeof(struct SimDNSResponse)];
            memcpy(responseBuffer, &response, sizeof(struct SimDNSResponse));

            // Create the packet
            char *packet = (char *)malloc(sizeof(struct ether_header) + sizeof(struct iphdr) + sizeof(struct SimDNSResponse));
            memcpy(packet, &ethHeaderResponse, sizeof(struct ether_header));
            memcpy(packet + sizeof(struct ether_header), &ipHeaderResponse, sizeof(struct iphdr));
            memcpy(packet + sizeof(struct ether_header) + sizeof(struct iphdr), responseBuffer, sizeof(struct SimDNSResponse));

            // Send the packet
            if (send(rawSocket, packet, sizeof(struct ether_header) + sizeof(struct iphdr) + sizeof(struct SimDNSResponse), 0) < 0)
            {
                perror("Send failed");
                exit(1);
            }

            printf("Served client %s\n", inet_ntoa(*(struct in_addr *)&ipHeader->saddr));
            fprintf(fp, "Served client %s\n", inet_ntoa(*(struct in_addr *)&ipHeader->saddr));

            // Garbage collection
            free(packet);
        }
    }

    // Garbage collection
    close(rawSocket);

    return 0;
}