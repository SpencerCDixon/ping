#pragma once

#include <sys/types.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>

#include "prelude.h"

internal u16 internet_checksum(void* addr, int len)
{
    int sum = 0;
    u16 answer = 0;
    u16* w = (u16*)addr;
    int nleft = len;

    // Our algorithm is simple, using a 32 bit accumulator (sum), we add
    // sequential 16 bit words to it, and at the end, fold back all the
    // carry bits from the top 16 bits into the lower 16 bits.
    while (nleft > 1) {
        sum += *w++;
        nleft -= 2;
    }

    /* mop up an odd byte, if necessary */
    if (nleft == 1) {
        *(char*)(&answer) = *(char*)w;
        sum += answer;
    }

    /* add back carry outs from top 16 bits to low 16 bits */
    sum = (sum >> 16) + (sum & 0xffff); /* add hi 16 to low 16 */
    sum += (sum >> 16);                 /* add carry */
    answer = ~sum;                      /* truncate to 16 bits */
    return answer;
}

typedef struct {
    struct icmphdr header;
    char msg[64 - sizeof(struct icmphdr)];
} ping_packet;

typedef struct {
    char ip[20];
    struct icmphdr header;
    char msg[64 - sizeof(struct icmphdr) - 20];
} pong_packet;

typedef struct {
    int bytes_sent;
    u16 sequence_num;
    s64 time_in_ms;
} echo_stat;

u16 packet_id()
{
    return (u16)getpid();
}

ping_packet init_ping_packet(u16 sequence_num)
{
    ping_packet ping_packet;
    memset(&ping_packet, 0, sizeof(ping_packet));

    ping_packet.header.type = ICMP_ECHO;
    ping_packet.header.code = 0;
    ping_packet.header.un.echo.id = htons(packet_id());
    ping_packet.header.un.echo.sequence = htons(sequence_num);
    strcpy(ping_packet.msg, "echo packet\n");
    ping_packet.header.checksum = internet_checksum(&ping_packet, sizeof(ping_packet));

    return ping_packet;
}

echo_stat* init_stat(u16 sequence_num)
{
    echo_stat* stat = (echo_stat*)malloc(sizeof(echo_stat));
    memset(stat, 0, sizeof(echo_stat));
    stat->sequence_num = sequence_num;
    return stat;
}

void print_stat(echo_stat* stat)
{
    if (stat->bytes_sent == 0)
        return;

    printf("%d bytes sent: icmp_seq %d time=%ld ms\n", stat->bytes_sent, (stat->sequence_num + 1), stat->time_in_ms);
}

struct sockaddr_in host_address(const char* host_name)
{
    struct hostent* he;
    if ((he = gethostbyname(host_name)) == NULL) {
        herror("gethostbyname");
        error_and_exit("require a hostname to ping");
    }

    int address_count = 0;
    struct in_addr** addresses = (struct in_addr**)he->h_addr_list;

#if 0
        printf("Official host name for %s is %s\n\n", host_name, he->h_name);
        printf("IP Addresses:\n");
        for (int i = 0; addresses[i] != NULL; i++) {
            printf("* %s\n", inet_ntoa(*addresses[i]));
            address_count++;
        }
        printf("\n");
#endif

    if (address_count > 1) {
        printf("warning: detected more than 1 IP address but using the first found\n");
    }

    struct sockaddr_in peer_address;
    memset(&peer_address, 0, sizeof(peer_address));
    peer_address.sin_family = AF_INET;
    peer_address.sin_port = 0;
    peer_address.sin_addr.s_addr = (in_addr_t)addresses[0]->s_addr;

    return peer_address;
}

int set_socket_timeout(const int fd, int secs)
{
    struct timeval timeout;
    timeout.tv_sec = secs;
    timeout.tv_usec = 0;
    int rc = setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    if (rc < 0)
        return -1;

    return 0;
}
