#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gprintf.h"
#include "limits.h"
#define SIX_BIT 6


static char const b64a[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                           "abcdefghijklmnopqrstuvwxyz"
                           "0123456789"
                           "+/";
static char const pad_char = '=';

void 
base64encode(const char *data, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        // Shift three bytes into a dword
        int bytes = 1;
        unsigned long dword = data[i];

        // Second byte
        dword <<= CHAR_BIT;
        if (++i < n) {
            dword |= data[i];
            ++bytes;
        }

        // Third byte
        dword <<= CHAR_BIT;
        if (++i < n) {
            dword |= data[i];
            ++bytes;
        }

        // Process input, six bits at a time
        for (int j = 0; j < ((3 * CHAR_BIT) / SIX_BIT); ++j) {
            if ((j * SIX_BIT) > (bytes * CHAR_BIT)) {
                putchar(pad_char);
            } else {
                int idx = (dword >> (3 * CHAR_BIT - SIX_BIT)) & 0x3F;
                char c = b64a[idx];
                putchar(c);
                dword <<= SIX_BIT; // Left shift
                dword &= 0xFFFFFF;     // Discard upper bits > 24th position
            }
        }
        
    }
    putchar('\n');
}

int 
main(int argc, char *argv[]) {
    FILE *file;

    /* Check for command line arguments */
    if (argc > 1) {
        file = fopen(argv[1], "rb");
        if (file == NULL) {
            perror("Error opening file"); /* Print error to stderr */
            return EXIT_FAILURE;
        }
    } else {
        file = stdin;
    }

    /* Read and encode input in chunks */
    char chunk[4096];  // Size used based on info from cat.c
    size_t bytesRead;

    while ((bytesRead = fread(chunk, 1, sizeof(chunk), file)) > 0) {
        base64_encode_chunk(chunk, bytesRead);
    }

    /* Close the file if not stdin */
    if (file != stdin) {
        fclose(file);
    }

    

  printf("The base64 alphabet is: %s\n", b64a);
  puts("Made it to the end! Goodbye :)");

  return EXIT_SUCCESS;
  return 0;
}
