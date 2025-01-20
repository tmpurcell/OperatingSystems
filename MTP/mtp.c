// Simplified program to test input output 
// for debugging

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

#define BUFFER_SIZE 50
#define LINE_LENGTH 1000

char *buffer[BUFFER_SIZE];
int count = 0;
int prod_idx = 0;
int con_idx = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
int done = 0;

void put_buffer(char *line) {
    pthread_mutex_lock(&mutex);
    while (count >= BUFFER_SIZE) {
        pthread_cond_wait(&cond, &mutex);
    }
    buffer[prod_idx] = line;
    prod_idx = (prod_idx + 1) % BUFFER_SIZE;
    count++;
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&mutex);
}

char *get_buffer() {
    pthread_mutex_lock(&mutex);
    while (count == 0 && !done) {
        pthread_cond_wait(&cond, &mutex);
    }
    if (count == 0 && done) {
        pthread_mutex_unlock(&mutex);
        return NULL;
    }
    char *line = buffer[con_idx];
    con_idx = (con_idx + 1) % BUFFER_SIZE;
    count--;
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&mutex);
    return line;
}

void *input_thread(void *arg) {
    FILE *input_stream = (FILE *)arg;
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    while ((read = getline(&line, &len, input_stream)) != -1) {
        put_buffer(strdup(line));
    }
    put_buffer(NULL); // Send termination signal
    free(line); // Cleanup getline allocated memory
    return NULL;
}

void *output_thread(void *arg) {
    (void)arg;
    char *line;
    while ((line = get_buffer()) != NULL) {
        printf("%s", line);
        free(line); // Free the line after printing
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    FILE *input_file = stdin;
    if (argc > 1) {
        input_file = fopen(argv[1], "r");
        if (!input_file) {
            perror("Could not open input file");
            return EXIT_FAILURE;
        }
    }

    pthread_t input_t, output_t;
    pthread_create(&input_t, NULL, input_thread, input_file);
    pthread_create(&output_t, NULL, output_thread, NULL);

    pthread_join(input_t, NULL);
    pthread_join(output_t, NULL);

    if (input_file != stdin) {
        fclose(input_file);
    }

    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&cond);

    return EXIT_SUCCESS;
}