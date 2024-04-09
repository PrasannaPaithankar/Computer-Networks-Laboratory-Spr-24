/*
 *  Author:     Prasanna Paithankar
 *  Roll No.:   21CS30065
 *  Course:     Computer Networks Laboratory (CS39006) Spr 2023-24
 *  Date:       08/04/2024

 *  File:       simDNS.h
 */

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
#include <linux/if.h>
#include <sys/select.h>
#include <errno.h>

#define P 0.0   // Probability of packet loss

struct
SimDNSQuery
{
    uint16_t id;                // ID: 16 bits
    uint8_t messageType : 1;    // Message Type: 1 bit (0 for query, 1 for response)
    uint8_t numQueries : 3;     // Number of queries: 3 bits
    struct
    {
        char domainName[32];    // Domain name: 32 bytes, first 4 bytes are the size of the domain name
    } queries[8];               // Maximum of 8 queries
} __attribute__((packed));

struct
SimDNSResponse
{
    uint16_t id;                // ID: 16 bits
    uint8_t messageType : 1;    // Message Type: 1 bit (0 for query, 1 for response)
    uint8_t numResponses : 3;   // Number of responses: 3 bits
    struct
    {
        uint8_t valid : 1;      // Flag bit indicating valid response
        in_addr_t ipAddress;    // IP address: 32 bits
    } responses[8];             // Maximum of 8 responses
} __attribute__((packed));

struct
ClientCache
{
    in_addr_t ipAddress;        // IP address: 32 bits
    uint16_t id;                // ID: 16 bits
};
