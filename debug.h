#include "prelude.h"
#include <stdio.h>

void hexdump(u8* object, size_t size)
{
    for (size_t i = 0; i < size; ++i) {
        if ((i % 16) == 0 && i != 0)
            printf("\n");

        printf("%02X ", *(object + i));
    }

    putchar('\n');
}
