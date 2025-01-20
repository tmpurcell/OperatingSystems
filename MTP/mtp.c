#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

// Size of the buffers
#define BUFFER_SIZE 50
#define LINE_LENGTH 1000

// Stop word
#define STOP_WORD "STOP\n"

// Buffer 1: shared resource between input thread and line separator thread
char* buffer_1[BUFFER_SIZE];
int count_1 = 0;
int prod_idx_1 = 0;
int con_idx_1 = 0;
pthread_mutex_t mutex_1 = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t full_1 = PTHREAD_COND_INITIALIZER;

// Buffer 2: sharing resources between line separator thread and plus sign thread
char* buffer_2[BUFFER_SIZE];
int count_2 = 0;
int prod_idx_2 = 0;
int con_idx_2 = 0;
pthread_mutex_t mutex_2 = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t full_2 = PTHREAD_COND_INITIALIZER;

// Buffer 3: shared resource between plus sign thread and output thread
char* buffer_3[BUFFER_SIZE];
int count_3 = 0;
int prod_idx_3 = 0;
int con_idx_3 = 0;
pthread_mutex_t mutex_3 = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t full_3 = PTHREAD_COND_INITIALIZER;

void put_buff_1(char* item) {
    pthread_mutex_lock(&mutex_1);
    // Wait if buffer is full
    while (count_1 == BUFFER_SIZE)
        pthread_cond_wait(&full_1, &mutex_1);
    buffer_1[prod_idx_1] = item;
    prod_idx_1 = (prod_idx_1 + 1) % BUFFER_SIZE;
    count_1++;
    // Signal that buffer has data
    pthread_cond_signal(&full_1);
    pthread_mutex_unlock(&mutex_1);
}

char* get_buff_1() {
    pthread_mutex_lock(&mutex_1);
    while (count_1 == 0)
        pthread_cond_wait(&full_1, &mutex_1);
    char* item = buffer_1[con_idx_1];
    con_idx_1 = (con_idx_1 + 1) % BUFFER_SIZE;
    count_1--;
    pthread_cond_signal(&full_1);
    pthread_mutex_unlock(&mutex_1);
    return item;
}

// Put an item in buffer 2
void put_buff_2(char *item) {
    pthread_mutex_lock(&mutex_2);
    while (count_2 == BUFFER_SIZE)
        pthread_cond_wait(&full_2, &mutex_2);
    buffer_2[prod_idx_2] = item;
    prod_idx_2 = (prod_idx_2 + 1) % BUFFER_SIZE;
    count_2++;
    pthread_cond_signal(&full_2);
    pthread_mutex_unlock(&mutex_2);
}

// Get the next item from buffer 2
char *get_buff_2() {
    pthread_mutex_lock(&mutex_2);
    while (count_2 == 0)
        pthread_cond_wait(&full_2, &mutex_2);
    char *item = buffer_2[con_idx_2];
    con_idx_2 = (con_idx_2 + 1) % BUFFER_SIZE;
    count_2--;
    pthread_cond_signal(&full_2);
    pthread_mutex_unlock(&mutex_2);
    return item;
}

// Put an item in buffer 3
void put_buff_3(char *item) {
    pthread_mutex_lock(&mutex_3);
    while (count_3 == BUFFER_SIZE)
        pthread_cond_wait(&full_3, &mutex_3);
    buffer_3[prod_idx_3] = item;
    prod_idx_3 = (prod_idx_3 + 1) % BUFFER_SIZE;
    count_3++;
    pthread_cond_signal(&full_3);
    pthread_mutex_unlock(&mutex_3);
}

// Get the next item from buffer 3
char *get_buff_3() {
    pthread_mutex_lock(&mutex_3);
    while (count_3 == 0)
        pthread_cond_wait(&full_3, &mutex_3);
    char *item = buffer_3[con_idx_3];
    con_idx_3 = (con_idx_3 + 1) % BUFFER_SIZE;
    count_3--;
    pthread_cond_signal(&full_3);
    pthread_mutex_unlock(&mutex_3);
    return item;
}

// Threads Start Here

void *input_thread(void *arg) {
    FILE *input_stream = (FILE *)arg;
    char *line = NULL;
    size_t len = 0;
    ssize_t nread;

    while ((nread = getline(&line, &len, input_stream)) != -1) {
        if (strcmp(line, STOP_WORD) == 0) {
            put_buff_1(NULL);
            break;
        } else {
            put_buff_1(line);
            line = NULL;
            len = 0;
        }
    }
    if (nread == -1) {
        put_buff_1(NULL);
    }
    free(line);
    return NULL;
}

void *line_separator_thread(void *arg) {
    char *line;
    while (1) {
        line = get_buff_1();
        if (line == NULL) {
            put_buff_2(NULL);
            return NULL;
        }
        for (int i = 0; line[i] != '\0'; i++) {
            if (line[i] == '\n') {
                line[i] = ' ';
            }
        }
        put_buff_2(line);
    }
}

void *plus_sign_thread(void *arg) {
    char *line;
    while (1) {
        line = get_buff_2();
        if (line == NULL) {
            // Forward termination signal
            put_buff_3(NULL);
            return NULL;
        }
        
        char *new_line = malloc(LINE_LENGTH);
        if (new_line == NULL) {
            fprintf(stderr, "Memory allocation failed in plus_sign_thread.\n");
            continue; 
        }
        
        int new_len = 0;
        int len = strlen(line);
        for (int i = 0; i < len; i++) {
            if (line[i] == '+' && i + 1 < len && line[i + 1] == '+') {
                new_line[new_len++] = '^';
                i++; // Skip the next '+' to replace "++" with "^"
            } else {
                new_line[new_len++] = line[i];
            }
        }
        // Null terminate
        new_line[new_len] = '\0'; 

        put_buff_3(new_line);
        free(line);
    }
}

void *output_thread(void *arg) {
    char output_line[81];
    int output_len = 0;

    while (1) {
        // Retrieve a line from buffer 3
        char *line = get_buff_3();
        if (line == NULL) {
            // Stop signal, flush remaining output
            if (output_len == 80) {
                write(STDOUT_FILENO, output_line, output_len);
                write(STDOUT_FILENO, "\n", 1);
            }
            break;
        }

        // Process the retrieved line and form 80 character output lines
        for (int i = 0; line[i] != '\0'; ++i) {
            output_line[output_len++] = line[i];
            if (output_len == 80) {
                // Once 80 characters, output the line
                write(STDOUT_FILENO, output_line, output_len);
                write(STDOUT_FILENO, "\n", 1); // Line separator
                output_len = 0; // Reset the output length for the next line
            }
        }

        free(line); // Free the memory allocated for the line
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    // Default to stdin unless a file is specified
    FILE *input_file = stdin;
    if (argc > 1) {
        input_file = fopen(argv[1], "r");
        if (!input_file) {
            perror("Could not open input file");
            return EXIT_FAILURE;
        }
    }

    pthread_t input_t, line_separator_t, plus_sign_t, output_t;
    pthread_create(&input_t, NULL, input_thread, input_file);
    pthread_create(&line_separator_t, NULL, line_separator_thread, NULL);
    pthread_create(&plus_sign_t, NULL, plus_sign_thread, NULL);
    pthread_create(&output_t, NULL, output_thread, NULL);

    pthread_join(input_t, NULL);
    pthread_join(line_separator_t, NULL);
    pthread_join(plus_sign_t, NULL);
    pthread_join(output_t, NULL);

    if (input_file != stdin) {
        fclose(input_file);
    }

    return EXIT_SUCCESS;
}
