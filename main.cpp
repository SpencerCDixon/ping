#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstdint>

void dump(uint8_t* object, size_t size)
{
    const unsigned char* byte;
    for (byte = object; size--; ++byte) {
        printf("%d", *byte);
    }
    putchar('\n');
}

uint16_t in_cksum(void* addr, int len)
{
    int sum = 0;
    uint16_t answer = 0;
    uint16_t* w = (uint16_t*)addr;
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

void shutdown(int sig)
{
    printf("\nshutting down ping command...\n");
    exit(1);
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
    if (fd < 0) {
        error("creating raw socket, make sure being run as 'sudo'");
    }
    return fd;
}

sockaddr_in host_address(const char* host_name, bool verbose)
{
    struct hostent* he;
    if ((he = gethostbyname(host_name)) == NULL) {
        herror("gethostbyname");
    }

    int address_count = 0;
    struct in_addr** addresses;
    addresses = (struct in_addr**)he->h_addr_list;
    if (verbose) {
        printf("Official host name for %s is %s\n\n", host_name, he->h_name);
        printf("IP Addresses:\n");
        for (int i = 0; addresses[i] != NULL; i++) {
            printf("* %s\n", inet_ntoa(*addresses[i]));
            address_count++;
        }
        printf("\n");
    }

    if (address_count > 1 && verbose) {
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

int main(int argc, char* argv[])
{
    char* host = argv[1];
    if (host == NULL) {
        printf("Usage: ping <host-name>\n");
        exit(1);
    }

    signal(SIGINT, shutdown);

    int fd = create_raw_socket();
    sockaddr_in peer_address = host_address(host, true);

    uint16_t seq = 0;
    struct PingPacket {
        struct icmphdr header;
        char msg[64 - sizeof(struct icmphdr)];
    };

    PingPacket ping_packet;
    memset(&ping_packet, 0, sizeof(PingPacket));

    pid_t pid = getpid();
    printf("running with pid %d\n", pid);
    ping_packet.header.type = ICMP_ECHO;
    ping_packet.header.code = 0;
    ping_packet.header.un.echo.id = htons(pid);
    ping_packet.header.un.echo.sequence = htons(seq++);
    strcpy(ping_packet.msg, "echo packet\n");
    ping_packet.header.checksum = in_cksum(&ping_packet, sizeof(PingPacket));

    int n = sendto(fd, &ping_packet, sizeof(PingPacket), 0, (const struct sockaddr*)&peer_address, sizeof(sockaddr_in));
    if (n < 0) {
        error("sendto -> sending echo ICMP packet");
    } else {
        printf("sent %d bytes\n", n);
        printf("awaiting echo response packet\n");
    }

    PingPacket pong_packet;
    memset(&pong_packet, 0, sizeof(PingPacket));

    while (1) {
        socklen_t address_size = sizeof(peer_address);
        int ret = recvfrom(fd, &pong_packet, sizeof(PingPacket), 0, (struct sockaddr*)&peer_address, &address_size);
        if (ret < 0)
            error("recvfrom");

        // Uhhh, what? https://blog.benjojo.co.uk/post/linux-icmp-type-69
        dump((uint8_t*)&pong_packet, sizeof(PingPacket));

        // if (pong_packet.header.type != ICMP_ECHOREPLY)
        // continue;

        // if (pong_packet.header.code != 0)
        // continue;

        // if (ntohs(pong_packet.header.un.echo.id) != pid)
        // continue;

        printf("Pong received!\n");
        break;
    }
    return 0;
}
