#ifndef FT_TRACEROUTE_H
# define FT_TRACEROUTE_H

# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <unistd.h>
# include <sys/time.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <netinet/ip.h>
# include <netinet/ip_icmp.h>
# include <netinet/udp.h>        /* struct udphdr */
# include <arpa/inet.h>
# include <netdb.h>

# define BASE_PORT 33434

/* utils.c */
unsigned short  checksum(void *b, int len);
int             resolve_host(char *host, char *ip, struct sockaddr_in *addr);

/* ft_traceroute.c */
void             create_sockets(int *send_fd, int *recv_fd);
void            send_packet(int sockfd, struct sockaddr_in *addr, int ttl, int probe);
int             receive_probe(int recv_fd, int ttl, int probe, double *time_ms, char *out_name, char *out_ip);
void            traceroute(char *host, char *ip, int send_fd, int recv_fd, struct sockaddr_in *addr);

#endif