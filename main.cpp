#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "prelude.h"

// -------
// Config
// -------

// Unique identifier so we can confirm that reply packets are indeed meant for us
#define PACKET_ID 619
// Extra logging for debugging
#define VERBOSE 0
// Number of times to ping the remote host to collect statistics
#define PING_COUNT 5

// -------
// Utility
// -------

void hexdump(u8* object, size_t size)
{
    for (size_t i = 0; i < size; ++i) {
        if ((i % 16) == 0 && i != 0)
            printf("\n");

        printf("%02X ", *(object + i));
    }

    putchar('\n');
}

u16 internet_checksum(void* addr, int len)
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
        *(u_char*)(&answer) = *(u_char*)w;
        sum += answer;
    }

    /* add back carry outs from top 16 bits to low 16 bits */
    sum = (sum >> 16) + (sum & 0xffff); /* add hi 16 to low 16 */
    sum += (sum >> 16);                 /* add carry */
    answer = ~sum;                      /* truncate to 16 bits */
    return answer;
}

void error(const char* msg)
{
    int len = strlen(msg);
    char buf[len + strlen("ERROR: ")];
    sprintf(buf, "ERROR: %s", msg);
    perror(buf);
    exit(1);
}

int create_raw_socket()
{
    int fd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (fd < 0)
        error("creating raw socket, make sure being run as 'sudo'");
    return fd;
}

sockaddr_in host_address(const char* host_name)
{
    struct hostent* he;
    if ((he = gethostbyname(host_name)) == NULL) {
        herror("gethostbyname");
    }

    int address_count = 0;
    struct in_addr** addresses = (struct in_addr**)he->h_addr_list;

    if (VERBOSE) {
        printf("Official host name for %s is %s\n\n", host_name, he->h_name);
        printf("IP Addresses:\n");
        for (int i = 0; addresses[i] != NULL; i++) {
            printf("* %s\n", inet_ntoa(*addresses[i]));
            address_count++;
        }
        printf("\n");
    }

    if (address_count > 1 && VERBOSE) {
        printf("detected more than 1 IP address but using the first found\n");
    }

    sockaddr_in peer_address;
    memset(&peer_address, 0, sizeof(peer_address));
    peer_address.sin_family = AF_INET;
    peer_address.sin_port = 0;
    // Andreas: Is this safe or is it going to bork when memory changes?
    peer_address.sin_addr.s_addr = (in_addr_t)addresses[0]->s_addr;

    return peer_address;
}

f64 now()
{
    struct timespec now;

    if ((clock_gettime(CLOCK_MONOTONIC, &now) < 0))
        perror("clock_gettime");

    // ts.tv_nsec rolls to 0 every second. We can get accurate measurement by multiplying
    // by the second
    f64 timestamp = now.tv_sec * 1000000000 + now.tv_nsec;
    return timestamp;
}

// -------
// Types
// -------

struct PingPacket {
    struct icmphdr header;
    char msg[64 - sizeof(struct icmphdr)];
};

struct PongPacket {
    char ip[20];
    struct icmphdr header;
    char msg[64 - sizeof(struct icmphdr) - 20];
};

struct EchoStats {
    int bytes_sent;
    u16 sequence_num;
    f64 time_in_ms;
};

PingPacket init_ping_packet(u16 sequence_num)
{
    PingPacket ping_packet;
    memset(&ping_packet, 0, sizeof(PingPacket));

    ping_packet.header.type = ICMP_ECHO;
    ping_packet.header.code = 0;
    ping_packet.header.un.echo.id = htons(PACKET_ID);
    ping_packet.header.un.echo.sequence = htons(sequence_num);
    strcpy(ping_packet.msg, "echo packet\n");
    ping_packet.header.checksum = internet_checksum(&ping_packet, sizeof(PingPacket));
    return ping_packet;
}

EchoStats* init_stat(u16 sequence_num)
{
    EchoStats* stat = (EchoStats*)malloc(sizeof(EchoStats));
    memset(stat, 0, sizeof(EchoStats));
    stat->sequence_num = sequence_num;
    return stat;
}

void print_stat(EchoStats* stat)
{
    if (stat->bytes_sent == 0)
        return;

    printf("%d bytes sent: icmp_seq %d time=%.1f ms\n", stat->bytes_sent, (stat->sequence_num + 1), stat->time_in_ms);
}

// -------
// Main
// -------

global EchoStats* stats[PING_COUNT];

void shutdown(int sig)
{
    printf("\nshutting down ping command...\n");

    int num_sent = 0;
    f64 total_time = 0.0f;
    for (size_t i = 0; i < PING_COUNT; ++i) {
        if (stats[i]->bytes_sent > 0) {
            num_sent++;
            total_time += stats[i]->time_in_ms;
        }
    }
    printf("-----Num pings sent: %d, average time: %.1f ms, total time: %.1f-----\n", num_sent, (total_time / num_sent), total_time);
    exit(1);
}

int main(int argc, char* argv[])
{
    // Arg parsing
    char* host = argv[1];
    if (host == NULL) {
        printf("Usage: ping <host-name>\n");
        exit(1);
    }

    // Setup
    signal(SIGINT, shutdown);

    int fd = create_raw_socket();
    sockaddr_in peer_address = host_address(host);

    // Fire away!
    u16 sequence_num = 0;
    for (size_t i = 0; i < PING_COUNT; ++i)
        stats[i] = init_stat(i);

    while (PING_COUNT > sequence_num) {
        EchoStats* stat = stats[sequence_num];

        PingPacket ping_packet = init_ping_packet(sequence_num++);

        f64 start_time = now();
        int sent = sendto(fd, &ping_packet, sizeof(PingPacket), 0, (const struct sockaddr*)&peer_address, sizeof(sockaddr_in));
        if (sent < 0) {
            error("sendto -> sending echo ICMP packet");
        } else {
            stat->bytes_sent = sent;
            if (VERBOSE)
                printf("awaiting echo response packet\n");
        }

        PongPacket pong_packet;
        memset(&pong_packet, 0, sizeof(PongPacket));

        while (1) {
            socklen_t address_size = sizeof(peer_address);
            int ret = recvfrom(fd, &pong_packet, sizeof(PongPacket), 0, (struct sockaddr*)&peer_address, &address_size);
            if (ret < 0)
                error("recvfrom");

            if (VERBOSE) {
                hexdump((u8*)&pong_packet, sizeof(PongPacket));
                printf("type: %d\n", pong_packet.header.type);
                printf("code: %d\n", pong_packet.header.code);
                printf("id: %d\n", ntohs(pong_packet.header.un.echo.id));
                printf("sequence: %d\n", ntohs(pong_packet.header.un.echo.sequence));
            }

            if (pong_packet.header.type != ICMP_ECHOREPLY)
                continue;

            if (pong_packet.header.code != 0)
                continue;

            if (ntohs(pong_packet.header.un.echo.id) != PACKET_ID)
                continue;

            f64 time_taken = (now() - start_time) / 1000000;
            stat->time_in_ms = time_taken;
            break;
        }

        print_stat(stat);
        sleep(1);
    }
    return 0;
}
