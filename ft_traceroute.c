#include "ft_traceroute.h"

void send_packet(int sockfd, struct sockaddr_in *addr, int ttl, int probe)
{
    // Each probe goes to a different port so we can match replies
    struct sockaddr_in dest = *addr;
    dest.sin_port = htons(33434 + (ttl - 1) * 3 + probe);

    setsockopt(sockfd, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl));

    char payload[60];
    memset(payload, 0, sizeof(payload));
    sendto(sockfd, payload, sizeof(payload), 0, (struct sockaddr *)&dest, sizeof(dest));
}
// Returns 1 if destination reached, 0 to continue, -1 on timeout/error
int receive_probe(int recv_fd, int ttl, int probe,
                double *time_ms, char *out_name, char *out_ip)
{
    char               buffer[1024];
    struct sockaddr_in sender;
    socklen_t          len = sizeof(sender);
    struct timeval     start, end;

    gettimeofday(&start, NULL);
    ssize_t size = recvfrom(recv_fd, buffer, sizeof(buffer), 0,
                            (struct sockaddr *)&sender, &len);
    gettimeofday(&end, NULL);

    if (size < 0)
        return -1;

    *time_ms = (end.tv_sec  - start.tv_sec)  * 1000.0
                + (end.tv_usec - start.tv_usec) / 1000.0;

    struct ip   *ip_hdr   = (struct ip *)buffer;
    struct icmp *icmp_hdr = (struct icmp *)(buffer + ip_hdr->ip_hl * 4);

    int type = icmp_hdr->icmp_type;
    int code = icmp_hdr->icmp_code;

    // We only care about TIME_EXCEEDED or PORT_UNREACH (destination reached)
    if (type == ICMP_TIME_EXCEEDED && code == ICMP_EXC_TTL) {
        // Verify inner UDP header matches our probe's destination port
        struct ip  *inner_ip  = (struct ip *)(buffer + ip_hdr->ip_hl * 4 + 8);
        struct udphdr *inner_udp = (struct udphdr *)((char *)inner_ip + inner_ip->ip_hl * 4);
        int expected_port = 33434 + (ttl - 1) * 3 + probe;
        if (ntohs(inner_udp->dest) != expected_port)
            return -1; // not our probe
    } else if (type == ICMP_DEST_UNREACH && code == ICMP_PORT_UNREACH) {
        // Destination host sent "port unreachable" = we arrived!
        struct ip  *inner_ip  = (struct ip *)(buffer + ip_hdr->ip_hl * 4 + 8);
        struct udphdr *inner_udp = (struct udphdr *)((char *)inner_ip + inner_ip->ip_hl * 4);
        int expected_port = 33434 + (ttl - 1) * 3 + probe;
        if (ntohs(inner_udp->dest) != expected_port)
            return -1;
        // Resolve and return "reached"
        strncpy(out_ip, inet_ntoa(sender.sin_addr), INET_ADDRSTRLEN);
        int r = getnameinfo((struct sockaddr *)&sender, sizeof(sender), out_name, 1024, NULL, 0, NI_NAMEREQD);
        if (r != 0)
            strncpy(out_name, out_ip, 1024);
        return 1; // destination reached
    }
    else
    {
        return -1;
    }

    strncpy(out_ip, inet_ntoa(sender.sin_addr), INET_ADDRSTRLEN);
    int r = getnameinfo((struct sockaddr *)&sender, sizeof(sender), out_name, 1024, NULL, 0, NI_NAMEREQD);
    if (r != 0) strncpy(out_name, out_ip, 1024);
    return 0;
}

void traceroute(char *host, char *ip, int send_fd, int recv_fd, struct sockaddr_in *addr)
{
    // Ajout du timeout au cas ou le paquet est perdu 3s
    struct timeval tv = { .tv_sec = 3, .tv_usec = 0 };
    setsockopt(recv_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    printf("traceroute to %s (%s), 30 hops max, 60 byte packets\n", host, ip);
    for (int ttl = 1; ttl <= 30; ttl++)
    {
        printf("%2d  ", ttl);
        fflush(stdout); // on ecrit sans attendre le buffer

        int  reached     = 0;
        int  printed_host = 0;
        char name[1024], sender_ip[INET_ADDRSTRLEN];

        for (int probe = 0; probe < 3; probe++)
        {
            send_packet(send_fd, addr, ttl, probe);

            double time_ms;
            int ret = receive_probe(recv_fd, ttl, probe,
                                    &time_ms, name, sender_ip);
            if (ret < 0)
                printf("* ");
            else 
            {
                if (!printed_host)
                {
                    printf("%s (%s)  ", name, sender_ip);
                    printed_host = 1;
                }
                printf("%.3f ms  ", time_ms);
                if (ret == 1)
                    reached = 1;
            }
            fflush(stdout);
        }
        printf("\n");
        if (reached) 
            break;
    }
}

int resolve_host(char *host, char *ip, struct sockaddr_in *addr)
{
    struct addrinfo hints = {0}, *res;
    
    hints.ai_family = AF_INET;
    
    if (getaddrinfo(host, NULL, &hints, &res) != 0)
        return (perror("getaddrinfo"), -1);
    
    *addr = *(struct sockaddr_in *)res->ai_addr;
    inet_ntop(AF_INET, &addr->sin_addr, ip, INET_ADDRSTRLEN);
    freeaddrinfo(res);
    return (0);
}

void create_sockets(int *send_fd, int *recv_fd)
{
    // socket pour mon raw UDP
    *send_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (*send_fd < 0)
        return (perror("send socket"));

    // socket pour mon raw ICMP
    *recv_fd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (*recv_fd < 0)
        return (perror("recv socket"));
}

int main(int ac, char **av)
{
    if (ac != 2)
    return (fprintf(stderr, "Usage: %s <host>\n", av[0]), 1);

    int recv_fd, send_fd;
    create_sockets(&send_fd, &recv_fd);
    if (send_fd < 0 || recv_fd < 0)
        return (1);

    char ip[INET_ADDRSTRLEN];
    struct sockaddr_in addr;
    if (resolve_host(av[1], ip, &addr) < 0) 
        return (1);

    traceroute(av[1], ip, send_fd, recv_fd, &addr);
    return (0);
}