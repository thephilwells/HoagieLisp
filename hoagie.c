#include <stdio.h>
#include <stdlib.h>
#include <math.h>
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

long eval (mpc_ast_t*);
long eval_op (long, char*, long);

int main(int argc, char** argv) {
    // Create some parsers
    mpc_parser_t* Number    = mpc_new("number");
    mpc_parser_t* Operator  = mpc_new("operator");
    mpc_parser_t* Expr      = mpc_new("expr");
    mpc_parser_t* Hoagie    = mpc_new("hoagie");

    // Define them with the following language
    mpca_lang(MPCA_LANG_DEFAULT,
    "                                                     \
     number     : /-?[0-9]+/ ;                            \
     operator   : '+' | '-' | '*' | '/' | '%' | '^';      \
     expr       : <number> | '(' <operator> <expr>+ ')' ; \
     hoagie     : /^/ <operator> <expr>+ /$/ ;            \
    ",
    Number, Operator, Expr, Hoagie);

    /* Print Version and Exit information */
    puts("\nHoagieLisp Version 0.0.0.2");
    puts("Press ctrl-c to exit\n");

    /* In a never-ending loop */
    while (1) {

        /* Output your prompt */
        char* input = readline("\nhoagie> ");
        add_history(input);

        // Attempt to parse user input
        mpc_result_t r;
        if (mpc_parse("<stdin>", input, Hoagie, &r)) {
            // On success print the AST
            long result = eval(r.output);
            printf("%li\n", result);
            mpc_ast_delete(r.output);
        } else {
            // Otherwise print the error
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }
        free(input);
    }
    // Undefine and delete our parsers
    mpc_cleanup(4, Number, Operator, Expr, Hoagie);
    return 0;
}

long eval(mpc_ast_t* t) {
    // If tagged as number, return it directly
    if(strstr(t->tag, "number")) {
        return atoi(t->contents);
    }

    // The operator is always the second child
    char* op = t->children[1]->contents;

    // We store the third child in 'x'
    long x = eval(t->children[2]);

    // Iterate the remaining children, and combine results
    int i = 3;
    while (strstr(t->children[i]->tag, "expr")) {
        x = eval_op(x, op, eval(t->children[i]));
        i++;
    }
    return x;
}

// Use operator string to see which operation to perform
long eval_op(long x, char* op, long y) {
    if (strcmp(op, "+") == 0) { return x + y; }
    if (strcmp(op, "-") == 0) { return x - y; }
    if (strcmp(op, "*") == 0) { return x * y; }
    if (strcmp(op, "/") == 0) { return x / y; }
    if (strcmp(op, "%") == 0) { return x % y; }
    if (strcmp(op, "^") == 0) { return pow(x, y); }
    return 0;
}