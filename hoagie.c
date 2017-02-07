#include <stdio.h>
#include <stdlib.h>
#include "mpc.h"

/* If we are compiling on Windows, compile these functions */
#ifdef _WIN32
#include <string.h>

static char buffer[2048];

/* Fake readline function */
char* readline(char* prompt) {
    fputs(prompt, stdout);
    fgets(buffer, 2048, stdin);
    char* cpy = malloc(strlen(buffer)+1);
    strcpy(cpy, buffer);
    cpy[strlen(cpy)-1] = "\0";
    return cpy;
}

/* Fake add_history function */
void add_history(char* unused) {}

/* Otherwise include the real editline headers */
/* comoile using `cc -std=c99 -Wall hoagie.c -lreadline -o hoagie` */
#else
#include <editline/readline.h>
#endif

int main(int argc, char** argv) {

    /* Print Version and Exit information */
    puts("\nHoagieLisp Version 0.0.0.1");
    puts("Press ctrl-c to exit\n");

    /* In a never-ending loop */
    while (1) {

        /* Output your prompt */
        char* input = readline("\nhoagie> ");
        add_history(input);

        printf("You typed: %s", input);
        free(input);
    }

    return 0;
}
