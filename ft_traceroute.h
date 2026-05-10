#ifndef FT_TRACEROUTE_H
#define FT_TRACEROUTE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <netdb.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>

void fill_data(uint8_t *data, int size);
uint16_t checksum(void *data, int length);
void help_situation();
int parse_ttl_option(char *arg, int *ttl);
int is_option(int ac, char **av, int *ttl, int *count);
#endif