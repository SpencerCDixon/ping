#pragma once

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#include "prelude.h"

void hexdump(u8* object, size_t size)
{
    for (size_t i = 0; i < size; ++i) {
        if ((i % 16) == 0 && i != 0)
            printf("\n");

        printf("%02X ", object[i]);
    }

    putchar('\n');
}

s64 timestamp_ms()
{
    struct timeval tv;
    int rc = gettimeofday(&tv, NULL);
    if (rc == -1)
        return -1;

    return (s64)((s64)tv.tv_sec * 1000 + (s64)tv.tv_usec / 1000);
}

void error_and_exit(const char* msg)
{
    fprintf(stderr, "Error: %s\n", msg);
    exit(1);
}

void perror_and_exit(const char* msg)
{
    perror(msg);
    exit(1);
}
