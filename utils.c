#include "ft_traceroute.h"

void fill_data(uint8_t *data, int size)
{
    for (int i = 0; i < size; i++)
        data[i] = i;
}

uint16_t checksum(void *data, int length)
{
    uint16_t *buf = (uint16_t *)data;
    uint32_t sum = 0;

    while (length > 1) {
        sum += *buf++;
        length -= 2;
    }

    if (length == 1) {
        sum += *((uint8_t *)buf);
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return (~sum);
}

static void print_help(char *prog)
{
    printf("Usage: %s [OPTIONS] <host>\n", prog);
    printf("\n");
    printf("Options:\n");
    printf("  -m <max_ttl>   Max number of hops (default: 30)\n");
    printf("  -q <nqueries>  Number of probes per hop (default: 3)\n");
    printf("  -p <port>      Base UDP destination port (default: 33434)\n");
    printf("  -n             Do not resolve IP addresses to hostnames\n");
    printf("  --help         Show this help\n");
}

int parse_args(int ac, char **av, t_opts *opts, char **host)
{
    // valeurs par défaut
    opts->max_ttl  = 30;
    opts->nqueries = 3;
    opts->port     = 33434;
    opts->no_dns   = 0;
    *host          = NULL;

    for (int i = 1; i < ac; i++)
    {
        if (strcmp(av[i], "--help") == 0)
        {
            print_help(av[0]);
            exit(0);
        }
        else if (strcmp(av[i], "-n") == 0)
            opts->no_dns = 1;
        else if (strcmp(av[i], "-m") == 0)
        {
            if (++i >= ac) 
                return (fprintf(stderr, "missing value for -m\n"), -1);
            opts->max_ttl = atoi(av[i]);
            if (opts->max_ttl <= 0)
                return (fprintf(stderr, "-m must be > 0\n"), -1);
        }
        else if (strcmp(av[i], "-q") == 0)
        {
            if (++i >= ac)
                return (fprintf(stderr, "missing value for -q\n"), -1);
            opts->nqueries = atoi(av[i]);
            if (opts->nqueries <= 0)
                return (fprintf(stderr, "-q must be > 0\n"), -1);
        }
        else if (strcmp(av[i], "-p") == 0)
        {
            if (++i >= ac)
                return (fprintf(stderr, "missing value for -p\n"), -1);
            opts->port = atoi(av[i]);
            if (opts->port <= 0 || opts->port > 65535)
                return (fprintf(stderr, "-p must be between 1 and 65535\n"), -1);
        }
        else if (av[i][0] == '-')
            return (fprintf(stderr, "unknown option: %s\n", av[i]), -1);
        else
        {
            if (*host)
                return (fprintf(stderr, "too many arguments\n"), -1);
            *host = av[i];
        }
    }
    if (!*host)
        return (fprintf(stderr, "Usage: %s [OPTIONS] <host>\n", av[0]), -1);
    int port_max = opts->port + opts->max_ttl * opts->nqueries - 1;
    if (port_max > 65535)
    {
        fprintf(stderr, "error: port range %d-%d exceeds 65535\n",
                opts->port, port_max);
        fprintf(stderr, "       reduce -p, -m or -q\n");
        return (-1);
    }
    return (0);
}