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
