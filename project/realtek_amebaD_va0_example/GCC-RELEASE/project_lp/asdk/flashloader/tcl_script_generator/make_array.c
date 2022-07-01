/*
 * Copyright (C) 2016 Rebane, rebane@alkohol.ee
 *
 * Build: gcc make_array.c -o make_array
 *
 */

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

int main() {
    ssize_t i, l;
    uint32_t value, index;
    uint8_t buffer[24];
    index = 0;
    printf("#\n");
    printf("# Flash loader binary\n");
    printf("#\n");
    printf("array set rtl872x_flash_loader {\n");
    while (1) {
        l = read(0, buffer, 24);
        if (l < 1)
            break;
        printf("\t");
        for (i = 0; i < l; i += 4) {
            value = ((uint32_t)buffer[i + 0] << 0) | ((uint32_t)buffer[i + 1] << 8) | ((uint32_t)buffer[i + 2] << 16) | ((uint32_t)buffer[i + 3] << 24);
            if (i)
                printf(" ");
            printf("%d 0x%08X", index++, (unsigned int)value);
        }
        printf("\n");
    }
    printf("}\n");
}
