#include "ft_traceroute.h"

void send_packet(char *host, int sockfd, struct sockaddr_in *addr, int seq)
{
    (void)host;
    char packet[64];
    memset(packet, 0, sizeof(packet));

    struct icmp *icmp_hdr = (struct icmp *)packet;

    icmp_hdr->icmp_type = ICMP_ECHO;
    icmp_hdr->icmp_code = 0;
    icmp_hdr->icmp_id = getpid();
    icmp_hdr->icmp_seq = htons(seq);

    icmp_hdr->icmp_cksum = 0;
    icmp_hdr->icmp_cksum = checksum(packet, sizeof(packet));

    sendto(sockfd, packet, sizeof(packet), 0,
           (struct sockaddr *)addr,
           sizeof(*addr));
}

int create_socket()
{
    int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sockfd < 0)
    {
        perror("socket");
        return -1;
    }
    return (sockfd);
}

int resolve_host(char *host, char *ip, struct sockaddr_in *addr)
{
    struct addrinfo hints = {0}, *res;

    hints.ai_family = AF_INET;

    if (getaddrinfo(host, NULL, &hints, &res) != 0)
        return (perror("getaddrinfo"), -1);

    *addr = *(struct sockaddr_in *)res->ai_addr;
    struct sockaddr_in *ipv4 = (struct sockaddr_in *)res->ai_addr;
    inet_ntop(AF_INET, &ipv4->sin_addr, ip, INET_ADDRSTRLEN);
    freeaddrinfo(res);
    return (0);
}

void traceroute(char *host, char *ip, int sockfd, struct sockaddr_in *addr)
{
    printf("traceroute to %s (%s), 30hops max, 60 byte packets\n", host, ip);

    for (int ttl = 1; ttl <= 30; ttl++)
    {
        setsockopt(sockfd, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl));

        struct timeval start, end;
        gettimeofday(&start, NULL);

        send_packet(host, sockfd, addr, ttl);

        char buffer[1024];
        struct sockaddr_in sender;
        socklen_t len = sizeof(sender);

        ssize_t size = recvfrom(sockfd, buffer, sizeof(buffer), 0,
                                (struct sockaddr *)&sender, &len);

        if (size < 0)
        {
            printf("%2d  *\n", ttl);
            continue;
        }

        gettimeofday(&end, NULL);

        double time_ms = (end.tv_sec - start.tv_sec) * 1000.0 +
            (end.tv_usec - start.tv_usec) / 1000.0;

        struct ip *ip_hdr = (struct ip *)buffer;
        struct icmp *icmp_hdr = (struct icmp *)(buffer + ip_hdr->ip_hl * 4);

        char gatewayName[1024];
        int ret = getnameinfo((struct sockaddr *)&sender, sizeof(sender), gatewayName, sizeof(gatewayName), NULL, 0, NI_NAMEREQD);
        if (ret != 0)
            printf("%2d  %s (%s)  %.3f ms\n", ttl, inet_ntoa(sender.sin_addr), inet_ntoa(sender.sin_addr),
               time_ms);
        else
            printf("%2d  %s (%s)  %.3f ms\n", ttl, gatewayName, inet_ntoa(sender.sin_addr),
                time_ms);
        // la destination
        if (icmp_hdr->icmp_type == ICMP_ECHOREPLY)
        {
            printf("destination reached\n");
            break;
        }

        // option debug utile
        if (icmp_hdr->icmp_type == ICMP_TIME_EXCEEDED)
        {
            // nos routeurs intermediaires
        }
    }
}

int main(int ac, char **av)
{
    if (ac != 2)
    {
        fprintf(stderr, "Usage: %s <host>\n", av[0]);
        return (1);
    }

    char *host = av[1];

    int sockfd = create_socket();
    if (sockfd < 0)
        return (1);

    char ip[INET_ADDRSTRLEN];
    struct sockaddr_in addr;
    if (resolve_host(host, ip, &addr) < 0)
        return (1);

    traceroute(host, ip, sockfd, &addr);

    return (0);
}