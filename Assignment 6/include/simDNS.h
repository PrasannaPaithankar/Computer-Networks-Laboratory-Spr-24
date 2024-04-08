#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/ether.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <arpa/inet.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>
#include <net/if_arp.h>
#include <net/if_packet.h>
#include <netdb.h>
#include <signal.h>
#include <sys/select.h>
#include <errno.h>

#define INTERFACE "lo"
#define SRC_MAC "00:00:00:00:00:00"
#define DST_MAC "00:00:00:00:00:00"

struct SimDNSQuery
{
    uint16_t id;             // ID: 16 bits
    uint8_t messageType : 1; // Message Type: 1 bit
    uint8_t numQueries : 3;  // Number of queries: 3 bits
    struct
    {
        char domainName[32]; // Domain name: 32 bytes, first 4 bytes are the size of the domain name
    } queries[8];            // Maximum of 8 queries
} __attribute__((packed));


struct SimDNSResponse
{
    uint16_t id;              // ID: 16 bits
    uint8_t messageType : 1;  // Message Type: 1 bit
    uint8_t numResponses : 3; // Number of responses: 3 bits
    struct
    {
        uint8_t valid : 1;  // Flag bit indicating valid response
        in_addr_t ipAddress; // IP address: 32 bits
    } responses[8];         // Maximum of 8 responses
} __attribute__((packed));


int
dropmessage(float prob);

void
processQuery(struct SimDNSQuery *query, struct SimDNSResponse *response);
