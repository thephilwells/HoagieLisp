#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "mpc.h"

/* If we are compiling on Windows, compile these functions */
#ifdef _WIN32
#include <string.h>

static char buffer[2048];

/* Fake readline function */
char *readline(char *prompt)
{
    fputs(prompt, stdout);
    fgets(buffer, 2048, stdin);
    char *cpy = malloc(strlen(buffer) + 1);
    strcpy(cpy, buffer);
    cpy[strlen(cpy) - 1] = "\0";
    return cpy;
}

/* Fake add_history function */
void add_history(char *unused) {}

/* Otherwise include the real editline headers */
/* comoile using `cc -std=c99 -Wall hoagie.c -lreadline -o hoagie` */
#else
#include <editline/readline.h>
#endif

typedef struct
{
    int type;
    double num;
    int err;
} lval;

lval eval(mpc_ast_t *);
lval eval_op(lval, char *, lval);
lval lval_num(double);
lval lval_err(int);
void lval_print(lval);
void lval_println(lval);

// Create enumeration of possible lval types
enum
{
    LVAL_NUM,
    LVAL_ERR
};

// Create enumeration of possible error types
enum
{
    LERR_DIV_ZERO,
    LERR_BAD_OP,
    LERR_BAD_NUM
};

int main(int argc, char **argv)
{
    // Create some parsers
    mpc_parser_t *Number = mpc_new("number");
    mpc_parser_t *Operator = mpc_new("operator");
    mpc_parser_t *Expr = mpc_new("expr");
    mpc_parser_t *Hoagie = mpc_new("hoagie");

    // Define them with the following language
    mpca_lang(MPCA_LANG_DEFAULT,
    "                                                                   \
     number     : /[-+]?\\d+(\\.\\d+)?/ ;                               \
     operator   : '+' | '-' | '*' | '/' | '%' | '^' | \"min\" | \"max\";\
     expr       : <number> | '(' <operator> <expr>+ ')' ;               \
     hoagie     : /^/ <operator> <expr>+ /$/ ;                          \
    ",
              Number, Operator, Expr, Hoagie);

    /* Print Version and Exit information */
    puts("\nHoagieLisp Version 0.0.0.8");
    puts("Press ctrl-c to exit\n");

    /* In a never-ending loop */
    while (1)
    {

        /* Output your prompt */
        char *input = readline("\nhoagie> ");
        add_history(input);

        // Attempt to parse user input
        mpc_result_t r;
        if (mpc_parse("<stdin>", input, Hoagie, &r))
        {
            // On success print the AST
            lval result = eval(r.output);
            lval_println(result);
            mpc_ast_delete(r.output);
        }
        else
        {
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

// Create a new number type lval
lval lval_num(double x)
{
    lval v;
    v.type = LVAL_NUM;
    v.num = x;
    return v;
}

// Create a new error type lval
lval lval_err(int x)
{
    lval v;
    v.type = LVAL_ERR;
    v.err = x;
    return v;
}

// print an lval
void lval_print(lval v)
{
    switch (v.type)
    {
    // If `type` is a number, print it
    case LVAL_NUM:
        printf("%g", v.num);
        break;
    // If type is an error
    case LVAL_ERR:
        if (v.err == LERR_DIV_ZERO)
        {
            printf("Error: division by zero!");
        }
        if (v.err == LERR_BAD_OP)
        {
            printf("Error: invalid operator!");
        }
        if (v.err == LERR_BAD_NUM)
        {
            printf("Error: invalid number! (too big, maybe?)");
        }
        break;
    }
}

void lval_println(lval v) {
    lval_print(v);
    putchar('\n');
}

lval eval(mpc_ast_t *t)
{
    // If tagged as number, return it directly
    if (strstr(t->tag, "number"))
    {
        // check if there is some error in conversion
        errno = 0;
        double x = strtod(t->contents, NULL);
        return errno != ERANGE ? lval_num(x) : lval_err(LERR_BAD_NUM);
    }

    // The operator is always the second child
    char *op = t->children[1]->contents;

    // We store the third child in 'x'
    lval x = eval(t->children[2]);

    // Iterate the remaining children, and combine results
    int i = 3;
    while (strstr(t->children[i]->tag, "expr"))
    {
        x = eval_op(x, op, eval(t->children[i]));
        i++;
    }
    return x;
}

// Use operator string to see which operation to perform
lval eval_op(lval x, char *op, lval y)
{
    // If either value is an error, return it
    if (x.type == LVAL_ERR) {return x; }
    if (y.type == LVAL_ERR) {return y; }

    if (strcmp(op, "+") == 0) { return lval_num(x.num + y.num); }
    if (strcmp(op, "-") == 0) { return lval_num(x.num - y.num); }
    if (strcmp(op, "*") == 0) { return lval_num(x.num * y.num);; }

    if (strcmp(op, "/") == 0) { 
        return y.num == 0 ? lval_err(LERR_DIV_ZERO) : lval_num(x.num / y.num); 
        }

    if (strcmp(op, "%") == 0) { 
        return y.num == 0 ? lval_err(LERR_DIV_ZERO) : lval_num(fmod(x.num, y.num)); 
        }
    if (strcmp(op, "^") == 0) { return lval_num(pow(x.num, y.num)); }
    if (strcmp(op, "min") == 0) {
        return x.num <= y.num ? lval_num(x.num) : lval_num(y.num);
        }
    if (strcmp(op, "max") == 0) {
        return x.num >= y.num ? lval_num(x.num) : lval_num(y.num);
        }
    return lval_err(LERR_BAD_OP);
}