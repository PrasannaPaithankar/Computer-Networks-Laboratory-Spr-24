#include "../include/simDNS.h"

void signalHandler(int signum)
{
    if (signum == SIGINT)
    {
        write(1, "\nsimDNS Server shutting down...\n", 32);
        write(1, "\nAuthor: Prasanna Paithankar\n", 29);
        exit(0);
    }
}

int dropmessage(float prob)
{
    return (rand() < prob * RAND_MAX);
}

void processQuery(struct SimDNSQuery *query, struct SimDNSResponse *response)
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
        printf("Size: %d\n", size);

        // Get the domain name
        memcpy(domainName, query->queries[i].domainName + 4, size);
        domainName[size] = '\0';
        printf("Domain Name: %s\n", domainName);

        // Get the IP address
        host = gethostbyname(domainName);

        if (host == NULL)
        {
            response->responses[i].valid = 0;
            response->responses[i].ipAddress = 0;
        }
        else
        {
            printf("IP Address: %s\n", inet_ntoa(*(struct in_addr *)host->h_addr_list[0]));
            response->responses[i].valid = 1;
            response->responses[i].ipAddress = ((struct in_addr *)host->h_addr_list[0])->s_addr;
        }

        // Increment the number of responses
        response->numResponses++;
    }

    return;
}


int main() 
{
    // Register the signal handler
    signal(SIGINT, signalHandler);
    
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
    sll.sll_ifindex = if_nametoindex("lo");
    inet_pton(AF_INET, "127.0.0.1", sll.sll_addr);

    if (bind(rawSocket, (struct sockaddr *)&sll, sizeof(sll)) == -1) {
        perror("bind() failed");
        close(rawSocket);
        exit(1);
    }

    // Read all the packets that are received to this socket
    while (1)
    {
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
        printf("Received a packet\n");
        

        // If the protocol field is 254, then read the query header
        simDNSQuery = (struct SimDNSQuery *)(buffer + sizeof(struct ether_header) + sizeof(struct iphdr));

        // If it is a simDNS query, then parse the query strings
        if (simDNSQuery->messageType == 0)
        {
            // Generate a simDNS response
            processQuery(simDNSQuery, &response);

            // Send the simDNS response to the client which had sent the query (you can find out the client IP address from the IP header). 
            // The response should be sent to the client using the raw socket.
            // create the ethernet header
            struct ether_header ethHeaderResponse;
            memcpy(ethHeaderResponse.ether_dhost, ethHeader->ether_shost, 6);
            memcpy(ethHeaderResponse.ether_shost, ethHeader->ether_dhost, 6);
            ethHeaderResponse.ether_type = htons(ETH_P_IP);

            // create the IP header
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

            // create the simDNS response
            char responseBuffer[sizeof(struct SimDNSResponse)];
            memcpy(responseBuffer, &response, sizeof(struct SimDNSResponse));

            // create the packet
            char *packet = (char *)malloc(sizeof(struct ether_header) + sizeof(struct iphdr) + sizeof(struct SimDNSResponse));
            memcpy(packet, &ethHeaderResponse, sizeof(struct ether_header));
            memcpy(packet + sizeof(struct ether_header), &ipHeaderResponse, sizeof(struct iphdr));
            memcpy(packet + sizeof(struct ether_header) + sizeof(struct iphdr), responseBuffer, sizeof(struct SimDNSResponse));

            // if (dropmessage(1))
            // {
            //     printf("Dropping the packet\n");
            //     continue;
            // }

            // send the packet
            if (send(rawSocket, packet, sizeof(struct ether_header) + sizeof(struct iphdr) + sizeof(struct SimDNSResponse), 0) < 0)
            {
                perror("Send failed");
                exit(1);
            }

            // free the packet
            free(packet);
        }
    }

    // Close the raw socket
    close(rawSocket);

    return 0;
}