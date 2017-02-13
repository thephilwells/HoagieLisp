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

typedef struct lval
{
    int type;
    double num;
    // Error and Symbol types have some string data
    char* err;
    char* sym;
    // Count and pointer to a list of "lval*"
    int count;
    struct lval** cell;
} lval;

// lval eval(mpc_ast_t *);
// lval eval_op(lval, char *, lval);
lval* lval_num(double);
lval* lval_err(char*);
lval* lval_sym(char*);
lval* lval_sexpr(void);
void lval_del(lval*);
lval* lval_read_num(mpc_ast_t*);
lval* lval_read(mpc_ast_t*);
lval* lval_add(lval*, lval*);
void lval_expr_print(lval*, char, char);
void lval_print(lval* v);
void lval_println(lval*);

// Create enumeration of possible lval types
enum
{
    LVAL_NUM,
    LVAL_ERR,
    LVAL_SYM,
    LVAL_SEXPR
};

int main(int argc, char **argv)
{
    // Create some parsers
    mpc_parser_t *Number = mpc_new("number");
    mpc_parser_t *Symbol = mpc_new("symbol");
    mpc_parser_t *Sexpr = mpc_new("sexpr");
    mpc_parser_t *Expr = mpc_new("expr");
    mpc_parser_t *Hoagie = mpc_new("hoagie");

    // Define them with the following language
    mpca_lang(MPCA_LANG_DEFAULT,
    "                                                                   \
     number     : /[-+]?\\d+(\\.\\d+)?/ ;                               \
     symbol     : '+' | '-' | '*' | '/' | '%' | '^' | \"min\" | \"max\";\
     sexpr      : '(' <expr>* ')' ;                                      \
     expr       : <number> | <symbol> | <sexpr> ;               \
     hoagie     : /^/ <expr>* /$/ ;                          \
    ",
    Number, Symbol, Sexpr, Expr, Hoagie);

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
            lval* x = lval_read(r.output);
            lval_println(x);
            lval_del(x);
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
    mpc_cleanup(5, Number, Symbol, Sexpr, Expr, Hoagie);
    return 0;
}

// Construct a pointer to a new Number lval
lval* lval_num(double x)
{
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_NUM;
    v->num = x;
    return v;
}

// Construct a pointer to a new Error lval
lval* lval_err(char* m)
{
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_ERR;
    v->err = malloc(strlen(m) + 1);
    strcpy(v->err, m);
    return v;
}

// Construct a pointer to a new Symbol lval
lval* lval_sym(char* s) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_SYM;
    v->sym = malloc(strlen(s) + 1);
    strcpy(v->sym, s);
    return v;
}

lval* lval_sexpr(void) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_SEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}


void lval_del(lval* v) {
    switch(v->type) {
        // for Number type, do nothing special
        case LVAL_NUM: break;

        // for Err or Sym type, free the string data
        case LVAL_ERR: free(v->err); break;
        case LVAL_SYM: free(v->sym); break;

        // If Sexpr then delete all elements inside
        case LVAL_SEXPR:
        for (int i = 0; i < v->count; i++) {
            lval_del(v->cell[i]);
        }
        free(v->cell);
        break;
    }
    free(v);
}

lval* lval_read_num(mpc_ast_t* t) {
    errno = 0;
    double x = strtod(t->contents, NULL);
    return errno != ERANGE ? lval_num(x) : lval_err("invalid number");
}

lval* lval_read(mpc_ast_t* t) {
    // If Symbol or Number, return conversion to that type
    if (strstr(t->tag, "number")) { return lval_read_num(t); }
    if (strstr(t->tag, "symbol")) { return lval_sym(t->contents); }

    // If root (>) or sexpr, then create empty list
    lval* x = NULL;
    if (strcmp(t->tag, ">") == 0) { x = lval_sexpr(); }
    if (strstr(t->tag, "sexpr"))  { x = lval_sexpr(); }

    // Fill this list with any valid expression contained within
    for (int i = 0; i < t->children_num; i++) {
        if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
        if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
        if (strcmp(t->children[i]->contents, "{") == 0) { continue; }
        if (strcmp(t->children[i]->contents, "}") == 0) { continue; }
        if (strcmp(t->children[i]->tag,  "regex") == 0) { continue; }
        x = lval_add(x, lval_read(t->children[i]));
    }
    return x;
}

lval* lval_add(lval* v, lval* x) {
    v->count++;
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    v->cell[v->count-1] = x;
    return v;
}

void lval_expr_print(lval* v, char open, char close) {
    putchar(open);
    for (int i = 0; i < v->count; i++) {
        // Print value contained within
        lval_print(v->cell[i]);

        // Print trailing space unless this is last element
        if (i != (v->count-1)) {
            putchar(' ');
        }
    }
    putchar(close);
}

void lval_print(lval* v) {
    switch(v->type) {
        case LVAL_NUM:   printf("%g", v->num); break;
        case LVAL_ERR:   printf("Error: %s", v->err); break;
        case LVAL_SYM:   printf("%s", v->sym); break;
        case LVAL_SEXPR: lval_expr_print(v, '(', ')'); break;
    }
}

// // print an lval
// void lval_print(lval v)
// {
//     switch (v.type)
//     {
//     // If `type` is a number, print it
//     case LVAL_NUM:
//         printf("%g", v.num);
//         break;
//     // If type is an error
//     case LVAL_ERR:
//         if (v.err == LERR_DIV_ZERO)
//         {
//             printf("Error: division by zero!");
//         }
//         if (v.err == LERR_BAD_OP)
//         {
//             printf("Error: invalid operator!");
//         }
//         if (v.err == LERR_BAD_NUM)
//         {
//             printf("Error: invalid number! (too big, maybe?)");
//         }
//         break;
//     }
// }

void lval_println(lval* v) {
    lval_print(v);
    putchar('\n');
}

// lval eval(mpc_ast_t *t)
// {
//     // If tagged as number, return it directly
//     if (strstr(t->tag, "number"))
//     {
//         // check if there is some error in conversion
//         errno = 0;
//         double x = strtod(t->contents, NULL);
//         return errno != ERANGE ? lval_num(x) : lval_err(LERR_BAD_NUM);
//     }

//     // The operator is always the second child
//     char *op = t->children[1]->contents;

//     // We store the third child in 'x'
//     lval x = eval(t->children[2]);

//     // Iterate the remaining children, and combine results
//     int i = 3;
//     while (strstr(t->children[i]->tag, "expr"))
//     {
//         x = eval_op(x, op, eval(t->children[i]));
//         i++;
//     }
//     return x;
// }

// // Use operator string to see which operation to perform
// lval eval_op(lval x, char *op, lval y)
// {
//     // If either value is an error, return it
//     if (x.type == LVAL_ERR) {return x; }
//     if (y.type == LVAL_ERR) {return y; }

//     if (strcmp(op, "+") == 0) { return lval_num(x.num + y.num); }
//     if (strcmp(op, "-") == 0) { return lval_num(x.num - y.num); }
//     if (strcmp(op, "*") == 0) { return lval_num(x.num * y.num);; }

//     if (strcmp(op, "/") == 0) { 
//         return y.num == 0 ? lval_err(LERR_DIV_ZERO) : lval_num(x.num / y.num); 
//         }

//     if (strcmp(op, "%") == 0) { 
//         return y.num == 0 ? lval_err(LERR_DIV_ZERO) : lval_num(fmod(x.num, y.num)); 
//         }
//     if (strcmp(op, "^") == 0) { return lval_num(pow(x.num, y.num)); }
//     if (strcmp(op, "min") == 0) {
//         return x.num <= y.num ? lval_num(x.num) : lval_num(y.num);
//         }
//     if (strcmp(op, "max") == 0) {
//         return x.num >= y.num ? lval_num(x.num) : lval_num(y.num);
//         }
//     return lval_err(LERR_BAD_OP);
// }