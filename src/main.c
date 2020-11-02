#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "debug.h"
#include "net.h"
#include "prelude.h"

// -------
// Config
// -------

// Number of times to ping the remote host to collect statistics
#define PING_COUNT 5

global echo_stat* stats[PING_COUNT];

internal void on_sig_int(int sig)
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
    signal(SIGINT, on_sig_int);

    int fd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (fd < 0)
        error_and_exit("creating raw socket, make sure being run as 'sudo'");

    int rc = set_socket_timeout(fd, 1);
    if (rc < 0)
        error_and_exit("unable to set socket timeout");

    struct sockaddr_in peer_address = host_address(host);

    // Fire away!
    u16 sequence_num = 0;
    for (size_t i = 0; i < PING_COUNT; ++i)
        stats[i] = init_stat(i);

    while (PING_COUNT > sequence_num) {
        s64 start_time, end_time;
        echo_stat* stat = stats[sequence_num];

        ping_packet ping = init_ping_packet(sequence_num++);

        if ((start_time = timestamp_ms()) < 0) {
            fprintf(stderr, "unable to get start_time\n");
            exit(1);
        }

        int sent = sendto(fd, &ping, sizeof(ping_packet), 0, (const struct sockaddr*)&peer_address, sizeof(struct sockaddr_in));
        if (sent < 0) {
            error_and_exit("sendto -> sending echo ICMP packet");
        } else {
            stat->bytes_sent = sent;
        }

        pong_packet pong_packet;
        memset(&pong_packet, 0, sizeof(pong_packet));

        while (1) {
            socklen_t address_size = sizeof(peer_address);
            int ret = recvfrom(fd, &pong_packet, sizeof(pong_packet), 0, (struct sockaddr*)&peer_address, &address_size);
            if (ret < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    fprintf(stderr, "Peer request timed out (seq=%d)\n", ping.header.un.echo.sequence);
                    break;
                }
                error_and_exit("recvfrom");
            }

#if 0
                hexdump((u8*)&pong_packet, sizeof(pong_packet));
                printf("type: %d\n", pong_packet.header.type);
                printf("code: %d\n", pong_packet.header.code);
                printf("id: %d\n", ntohs(pong_packet.header.un.echo.id));
                printf("sequence: %d\n", ntohs(pong_packet.header.un.echo.sequence));
#endif

            if (pong_packet.header.type != ICMP_ECHOREPLY)
                continue;

            if (pong_packet.header.code != 0)
                continue;

            if (ntohs(pong_packet.header.un.echo.id) != PACKET_ID)
                continue;

            if ((end_time = timestamp_ms()) < 0) {
                fprintf(stderr, "unable to get start_time\n");
                exit(1);
            }
            stat->time_in_ms = end_time - start_time;
            break;
        }

        print_stat(stat);
        sleep(1);
    }
    return 0;
}
