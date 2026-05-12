#include "ft_traceroute.h"

void send_packet(int sockfd, struct sockaddr_in *addr, int ttl, int probe, t_opts *opts)
{
    struct sockaddr_in dest = *addr;
    dest.sin_port = htons(opts->port + (ttl - 1) * opts->nqueries + probe);

    setsockopt(sockfd, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl));

    char payload[60];
    memset(payload, 0, sizeof(payload));
    sendto(sockfd, payload, sizeof(payload), 0, (struct sockaddr *)&dest, sizeof(dest));
}

int receive_probe(int recv_fd, int ttl, int probe, double *time_ms, char *out_name, char *out_ip, t_opts *opts)
{
    char               buffer[1024];
    struct sockaddr_in sender;
    socklen_t          len = sizeof(sender);
    struct timeval     start, end;

    gettimeofday(&start, NULL);
    ssize_t size = recvfrom(recv_fd, buffer, sizeof(buffer), 0, (struct sockaddr *)&sender, &len);
    gettimeofday(&end, NULL);

    if (size < 0)
        return (-1); // on a timeout

    *time_ms = (end.tv_sec  - start.tv_sec)  * 1000.0
                + (end.tv_usec - start.tv_usec) / 1000.0;

    struct ip   *ip_hdr   = (struct ip *)buffer;
    struct icmp *icmp_hdr = (struct icmp *)(buffer + ip_hdr->ip_hl * 4);

    int type = icmp_hdr->icmp_type;
    int code = icmp_hdr->icmp_code;

    int expected_port = opts->port + (ttl - 1) * opts->nqueries + probe;
    if (type == ICMP_TIME_EXCEEDED && code == ICMP_EXC_TTL) 
    {
        struct ip  *inner_ip  = (struct ip *)(buffer + ip_hdr->ip_hl * 4 + 8);
        struct udphdr *inner_udp = (struct udphdr *)((char *)inner_ip + inner_ip->ip_hl * 4);
        if (ntohs(inner_udp->dest) != expected_port)
            return (-1);
    } 
    else if (type == ICMP_DEST_UNREACH && code == ICMP_PORT_UNREACH) 
    {
        struct ip  *inner_ip  = (struct ip *)(buffer + ip_hdr->ip_hl * 4 + 8);
        struct udphdr *inner_udp = (struct udphdr *)((char *)inner_ip + inner_ip->ip_hl * 4);
        if (ntohs(inner_udp->dest) != expected_port)
            return (-1);
        strncpy(out_ip, inet_ntoa(sender.sin_addr), INET_ADDRSTRLEN);
        if (opts->no_dns)
            strncpy(out_name, out_ip, 1024);
        else
        {
            int r = getnameinfo((struct sockaddr *)&sender, sizeof(sender), out_name, 1024, NULL, 0, NI_NAMEREQD);
            if (r != 0)
                strncpy(out_name, out_ip, 1024);
        }
        return (1); 
    }
    else
        return (-1);
    strncpy(out_ip, inet_ntoa(sender.sin_addr), INET_ADDRSTRLEN);
    if (opts->no_dns)
            strncpy(out_name, out_ip, 1024);
    else
    {
        int r = getnameinfo((struct sockaddr *)&sender, sizeof(sender), out_name, 1024, NULL, 0, NI_NAMEREQD);
        if (r != 0)
            strncpy(out_name, out_ip, 1024);
    }
    return (0);
}

void traceroute(char *host, char *ip, int send_fd, int recv_fd, struct sockaddr_in *addr, t_opts *opts)
{
    // Ajout du timeout au cas ou le paquet est perdu 3s test avec 192.168.0.0
    struct timeval tv = { .tv_sec = 3, .tv_usec = 0 };
    setsockopt(recv_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    printf("traceroute to %s (%s), %d hops max, 60 byte packets\n", host, ip, opts->max_ttl);
    for (int ttl = 1; ttl <= opts->max_ttl; ttl++)
    {
        printf("%2d  ", ttl);
        fflush(stdout); // on ecrit sans attendre le buffer

        int  reached = 0;
        int  printed_host = 0;
        char name[1024], sender_ip[INET_ADDRSTRLEN];

        for (int probe = 0; probe < opts->nqueries; probe++)
        {
            send_packet(send_fd, addr, ttl, probe, opts);

            double time_ms;
            int ret = receive_probe(recv_fd, ttl, probe, &time_ms, name, sender_ip, opts);
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
    t_opts opts;
    char *host;
    if (parse_args(ac, av, &opts, &host) < 0)
        return (1);

    int recv_fd, send_fd;
    create_sockets(&send_fd, &recv_fd);
    if (send_fd < 0 || recv_fd < 0)
        return (1);

    char ip[INET_ADDRSTRLEN];
    struct sockaddr_in addr;
    if (resolve_host(host, ip, &addr) < 0) 
        return (1);

    traceroute(host, ip, send_fd, recv_fd, &addr, &opts);
    return (0);
}