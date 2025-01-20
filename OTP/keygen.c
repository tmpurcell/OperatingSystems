#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Function to generate a random character
char generate_random_character() {
    // Allowed characters: A-Z & space
    // Generate a random number between 0 and 26
    int randomValue = rand() % 27; 
    if (randomValue == 26) {
        // Space is the 27th char
        return ' ';
    } else {
        // Return a character between A and Z
        return 'A' + randomValue;
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        // Exit with error code
        fprintf(stderr, "Usage: %s keylength\n", argv[0]);
        return 1;
    }

    int keyLength = atoi(argv[1]);
    if (keyLength <= 0) {
        // Exit with error code
        fprintf(stderr, "Error: Key length must be a positive integer\n");
        return 1;
    }

    // Initialize random number generator
    srand(time(NULL));

    // Generate key and output
    for (int i = 0; i < keyLength; i++) {
        putchar(generate_random_character());
    }

    // Last char is Newline
    putchar('\n');

    return 0;
}
