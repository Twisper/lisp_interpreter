/**
 * @file interpreter.c
 * @brief A simple Lisp interpreter from "Build your own Lisp" book. 
 *
 * @author Mikhail Ulyanov
 * @date Feb 2026
 *
 * @details
 * Uses mpc library for language parsing. 
 * Supports dynamic typisation, allowing to use floats and integers in one expression.
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <math.h>
#include "mpc.h"

/**
 * @brief Debug logging macro.
 * * Acts like printf but includes function name and line number.
 * Activated strictly via -DDEBUG flag. 
 * Use: LOG("Value: %d", val);
 */
#ifdef DEBUG
    #define LOG(fmt, ...) fprintf(stderr, "\n[LOG] %s:%d: " fmt "\n", __func__, __LINE__, ##__VA_ARGS__)
#else
    #define LOG(fmt, ...)
#endif

#define LASSERT(args, cond, fmt, ...) \
  if (!(cond)) { lval_t* err = lval_err(fmt, ##__VA_ARGS__); lval_del(args); return err; }

#define LASSERT_TYPE(func, args, index, expect) \
  LASSERT(args, args->cell[index]->type == expect, \
    "Function '%s' passed incorrect type for argument %i. " \
    "Got %s, Expected %s.", \
    func, index, ltype_name(args->cell[index]->type), ltype_name(expect))

#define LASSERT_NUM(func, args, num) \
  LASSERT(args, args->count == num, \
    "Function '%s' passed incorrect number of arguments. " \
    "Got %i, Expected %i.", \
    func, args->count, num)

#define LASSERT_NOT_EMPTY(func, args, index) \
  LASSERT(args, args->cell[index]->count != 0, \
    "Function '%s' passed {} for argument %i.", func, index);

/**
 * @brief 
 * This structure is used for storing parsed element using dynamic typisation.
 * It consists from two elements:
 * type shows variable type and union contains the variable. This also allows to use dynamic typisation.
*/

typedef struct lval lval_t;
typedef struct lenv lenv_t;

typedef enum var_types {LVAL_NUM, LVAL_FLOAT, LVAL_ERR, LVAL_SYM, LVAL_SEXPR, LVAL_QEXPR, LVAL_FUN} var_t; //Enum for different operand types. 

typedef lval_t*(*lbuiltin)(lenv_t*, lval_t*);

struct lenv {
    lenv_t* par;
    int count;
    char** syms;
    lval_t** vals;
};
struct lval {
    var_t type;
    
    union {
        long num;
        double dnum;
        char* err;
        char* sym;
        lbuiltin builtin;
    };

    lenv_t* env;
    lval_t* formals;
    lval_t* body;

    int count;
    struct lval** cell;
};

typedef enum err_types {LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM} err_t;

lval_t* lval_eval_sexpr(lenv_t* e, lval_t* v);
lval_t* lval_eval(lenv_t* e, lval_t* v);
lval_t *lval_num(long x);
lval_t *lval_float(double x);
lval_t* lval_err(char* fmt, ...);
lval_t* lval_sym(char* s);
lval_t* lval_sexpr(void);
lval_t* lval_qexpr(void);
lval_t* lval_fun(lbuiltin func);
lval_t* lval_lambda(lval_t* formals, lval_t* body);
lval_t* lval_call(lenv_t* e, lval_t* f, lval_t* a);

lval_t* builtin_add(lenv_t* e, lval_t* a);
lval_t* builtin_sub(lenv_t* e, lval_t* a);
lval_t* builtin_mul(lenv_t* e, lval_t* a);
lval_t* builtin_div(lenv_t* e, lval_t* a);
lval_t* builtin_pow(lenv_t* e, lval_t* a);
lval_t* builtin_mod(lenv_t* e, lval_t* a);
lval_t* builtin_min(lenv_t* e, lval_t* a);
lval_t* builtin_max(lenv_t* e, lval_t* a);
lval_t* builtin_lambda(lenv_t* e, lval_t* a);

lval_t* builtin_op(lenv_t* e, lval_t* a, char* op);
lval_t* builtin_head(lenv_t* e, lval_t* a);
lval_t* builtin_tail(lenv_t* e, lval_t* a);
lval_t* builtin_list(lenv_t* e, lval_t* a);
lval_t* builtin_eval(lenv_t* e, lval_t* a);
lval_t* builtin_join(lenv_t* e, lval_t* a);
lval_t* builtin_cons(lenv_t* e, lval_t* a);
lval_t* builtin_len(lenv_t* e, lval_t* a);
lval_t* builtin_init(lenv_t* e, lval_t* a);
lval_t* builtin_put(lenv_t* e, lval_t* a);
lval_t* builtin_def(lenv_t* e, lval_t* a);
lval_t* builtin_var(lenv_t* e, lval_t* a, char* func);
lval_t* lval_join(lval_t* x, lval_t* y);
lval_t* lval_copy(lval_t* v);

lenv_t* lenv_new(void);
void lenv_del(lenv_t* e);
lval_t* lenv_get(lenv_t* e, lval_t* k);
void lenv_put(lenv_t* e, lval_t* k, lval_t* v);
void lenv_add_builtin(lenv_t* e, char* name, lbuiltin func);
void lenv_add_builtins(lenv_t* e);
lenv_t* lenv_copy(lenv_t* e);
void lenv_def(lenv_t* e, lval_t* k, lval_t* v);


void lval_print(lval_t* res);
void lval_println(lval_t* res);
void lval_del(lval_t* v);

lval_t* lval_read_num(mpc_ast_t* t);
lval_t* lval_read_float(mpc_ast_t* t);
lval_t* lval_read(mpc_ast_t* t);
lval_t* lval_add(lval_t* v, lval_t* x);
lval_t* lval_pop(lval_t* v, int i);
lval_t* lval_take(lval_t* v, int i);

void lval_expr_print(lval_t* v, char open, char close);
long ipow(long base, int exp);

char* ltype_name(int t);

int main(int argc, char** argv) {

    //Telling parser what it can parse. 
    mpc_parser_t* Number   = mpc_new("number");
    mpc_parser_t* Float    = mpc_new("float");
    mpc_parser_t* Symbol   = mpc_new("symbol");
    mpc_parser_t* Sexpr    = mpc_new("sexpr");
    mpc_parser_t* Qexpr    = mpc_new("qexpr");
    mpc_parser_t* Expr     = mpc_new("expr");
    mpc_parser_t* TinyLisp = mpc_new("tinylisp");

    //Defining Lisp grammar. 
    mpca_lang(MPCA_LANG_DEFAULT,
    "                                                                     \
        number   : /-?[0-9]+/ ;                                            \
        float    : /-?[0-9]+[.][0-9]+/ ;                                    \
        symbol   : /[a-zA-Z0-9_+\\-*\\/\\^\\*\\\\\\=<>!&]+/ ;                    \
        sexpr    : '(' <expr>* ')' ;                                          \
        qexpr    : '{' <expr>* '}' ;                                             \
        expr     : <float> | <number> | <symbol> | <sexpr> | <qexpr> ;          \
        tinylisp : /^/ <expr>* /$/ ;                                             \
    ",
    Number, Float, Symbol, Sexpr, Qexpr, Expr, TinyLisp);
    
    //Printing version and exit information. 
    puts("TinyLisp Version 0.0.0.0.8");
    puts("Press Ctrl+C to Exit\n");

    lenv_t* e = lenv_new();
    lenv_add_builtins(e);
    
    while (1) {
    
        char* input = readline("tinylisp> "); //Outputing prompt and getting input. 
        
        add_history(input); //Adding input to history. 
        
        mpc_result_t r;
        if (mpc_parse("<stdin>", input, TinyLisp, &r)) { //Attempting to parse the user input. 
            //If succsessful, print the AST
            lval_t* result = lval_eval(e, lval_read(r.output));
            lval_println(result);
            lval_del(result);
            //mpc_ast_print(r.output);
            mpc_ast_delete(r.output);
        } else {
            //Otherwise printing the error and deleting result. 
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        } 

        free(input); //Free retrieved input. 
    
    }

    mpc_cleanup(7, Number, Float, Symbol, Sexpr, Qexpr, Expr, TinyLisp); //Undefining and deleting parsers   
    lenv_del(e);
  
    return 0;
}

lval_t* lval_eval_sexpr(lenv_t* e, lval_t* v) {

    for (int i = 0; i < v->count; i++) {
        v->cell[i] = lval_eval(e, v->cell[i]);
    }

    for (int i = 0; i < v->count; i++) {
        if (v->cell[i]->type == LVAL_ERR) { return lval_take(v, i); }
    }

    if (v->count == 0) { return v; }
    if (v->count == 1) { return lval_take(v, 0); }

    /* Ensure first element is a function after evaluation */
    lval_t* f = lval_pop(v, 0);
    if (f->type != LVAL_FUN) {
        lval_t* err = lval_err(
            "S-Expression starts with incorrect type. "
            "Got %s, Expected %s.",
            ltype_name(f->type), ltype_name(LVAL_FUN));
        lval_del(f); lval_del(v);
        return err;
    }

    lval_t* result = lval_call(e, f, v);
    lval_del(f);
    return result;
}

/**
 * @brief
 * This function evaluates the input from user. It recursively iterates through S-expression and returns pointer to the result of all operations.
*/

lval_t* lval_eval(lenv_t* e, lval_t* v) {
    if (v->type == LVAL_SYM) {
        lval_t* x = lenv_get(e, v);
        lval_del(v);
        return x;
    }
    if (v->type == LVAL_SEXPR) { return lval_eval_sexpr(e, v); }
    return v;
}

/**
 * @details
 * This function finds a result of S-expression with one operation and one or more operands
 * If one of the operands is double type, it makes double result. Some of double opeartions aren't same to long, that's why there are two branches for each case. 
 * Otherwise the result of operation is long type. 
 * The function creates structure of result and returns pointer to it, freeing memory of every operand before. 
*/

lval_t* builtin_op(lenv_t* e, lval_t* a, char* op) {
  
    for (int i = 0; i < a->count; i++) {
        if ((a->cell[i]->type != LVAL_NUM) && (a->cell[i]->type != LVAL_FLOAT)) {
            lval_del(a);
            return lval_err("Cannot operate on non-number!");
        }
    }

    //Popping the first element. 
    lval_t* x = lval_pop(a, 0);

    //If no arguments and sub then perform unary negation. 
    if ((strcmp(op, "-") == 0) && a->count == 0) {
        LOG("X type: %d", x->type);
        switch (x->type) {
            case LVAL_NUM: 
                x->num = -x->num; 
                break;
            case LVAL_FLOAT: 
                x->dnum = -x->dnum; 
                break;
        }
    }

    //While there are still elements remaining. 
    while (a->count > 0) {

        //Pop the next element
        lval_t* y = lval_pop(a, 0);

        if (((x->type == LVAL_FLOAT) && (y->type == LVAL_NUM)) || ((x->type == LVAL_NUM) && (y->type == LVAL_FLOAT)) || ((x->type == LVAL_FLOAT) && (y->type == LVAL_FLOAT))) { //If one of operands is double. 
            double op1 = (x->type == LVAL_FLOAT) ? x->dnum : (double)x->num;
            double op2 = (y->type == LVAL_FLOAT) ? y->dnum : (double)y->num;
            if (strcmp(op, "+") == 0) { x->dnum = op1 + op2;} //Adding. 
            else if (strcmp(op, "-") == 0) { x->dnum = op1 - op2; } //Subtracting. 
            else if (strcmp(op, "*") == 0) { x->dnum = op1 * op2; } //Multiplying. 
            else if (strcmp(op, "/") == 0) { x->dnum = op1 / op2; } //Dividing. 
            else if (strcmp(op, "%") == 0) { x->dnum = fmod(op1, op2); } //Dividing by module. 
            else if (strcmp(op, "^") == 0) { x->dnum = pow(op1, op2); } //Raising to a power. 
            else if (strcmp(op, "min") == 0) { x->dnum = (op1 > op2) ? op2 : op1; } //Finding minimum of two operands. 
            else if (strcmp(op, "max") == 0) { x->dnum = (op1 > op2) ? op1 : op2; } //Finding maximum of two operands. 
        } else if ((x->type == LVAL_NUM) && (y->type == LVAL_NUM)) { //Otherwise (two operands are long type) all same steps but specifically for long types.
            if (strcmp(op, "+") == 0) { x->num = x->num + y->num; } 
            else if (strcmp(op, "-") == 0) { x->num = x->num - y->num; }
            else if (strcmp(op, "*") == 0) { x->num = x->num * y->num; }
            else if (strcmp(op, "/") == 0) { 
                if (y->num == 0){
                    lval_del(x); lval_del(y);
                    x = lval_err("Division By Zero!"); break;
                }
                x->num /= y->num;
            }
            else if (strcmp(op, "%") == 0) { x->num = (x->num % y->num); }
            else if (strcmp(op, "^") == 0) { x->num = ipow(x->num, y->num); }
            else if (strcmp(op, "min") == 0) { x->num = (x->num > y->num) ? y->num : x->num; }
            else if (strcmp(op, "max") == 0) { x->num = (x->num > y->num) ? x->num : y->num; }
        }

        lval_del(y);
    }

    lval_del(a); 
    return x;
}

/**
 * @brief
 * This function implements binary power method in order to raise base to a power of exp with log complexity.
*/
long ipow(long base, int exp) {
    long res = 1;
    while (exp > 0) {
        if (exp % 2 == 1) res *= base;
        base *= base;
        exp /= 2;
    }
    return res;
}

/**
 * @brief
 * This function prints result of expression depending on type of result - double or long.
*/
void lval_print(lval_t* res) {
  switch (res->type) {
    case LVAL_NUM:   printf("%ld", res->num); break;
    case LVAL_FLOAT: printf("%f", res->dnum); break;
    case LVAL_ERR:   printf("Error: %s", res->err); break;
    case LVAL_SYM:   printf("%s", res->sym); break;
    case LVAL_SEXPR: lval_expr_print(res, '(', ')'); break;
    case LVAL_QEXPR: lval_expr_print(res, '{', '}'); break;
    case LVAL_FUN:
        if (res->builtin) {
            printf("<builtin>");
        } else {
            printf("(\\ "); lval_print(res->formals);
            putchar(' '); lval_print(res->body); putchar(')');
        }
        break;
  }
}

//This function prints an lval followed by a newline
void lval_println(lval_t* res) { lval_print(res); putchar('\n'); }

//This function frees memory of given operand
void lval_del(lval_t* v) {

    switch (v->type) {
        //Doing nothing for number or float types. 
        case LVAL_NUM: break;
        case LVAL_FLOAT: break;

        //For Err or Sym freeing the string data.
        case LVAL_ERR: free(v->err); break;
        case LVAL_SYM: free(v->sym); break;

        case LVAL_FUN:
            if (!v->builtin) {
                lenv_del(v->env);
                lval_del(v->formals);
                lval_del(v->body);
            }
            break;

        //If Sexpr then delete all elements inside.
        case LVAL_QEXPR:
        case LVAL_SEXPR:
            for (int i = 0; i < v->count; i++) {
                lval_del(v->cell[i]);
            }
            //Freeing the memory allocated to contain the pointers. 
            free(v->cell);
            break;
    }

    //Freeing the memory allocated for the lval_t struct itself. 
    free(v);
}

//This function creates structure of long type of number.
lval_t* lval_num(long x) {
    lval_t* v = (lval_t *)malloc(sizeof(lval_t));
    v->type = LVAL_NUM;
    v->num = x;
    return v;
}

//This function creates structure of double type of number. 
lval_t* lval_float(double x) {
    lval_t* v = (lval_t *)malloc(sizeof(lval_t));
    v->type = LVAL_FLOAT;
    v->dnum = x;
    return v;
}

//This function creates structure based on input error. 
lval_t* lval_err(char* fmt, ...) {
    lval_t* v = malloc(sizeof(lval_t));
    v->type = LVAL_ERR;

    /* Create a va list and initialize it */
    va_list va;
    va_start(va, fmt);

    /* Allocate 512 bytes of space */
    v->err = malloc(512);

    /* printf the error string with a maximum of 511 characters */
    vsnprintf(v->err, 511, fmt, va);

    /* Reallocate to number of bytes actually used */
    v->err = realloc(v->err, strlen(v->err)+1);

    /* Cleanup our va list */
    va_end(va);

    return v;
}

//This function creates structure of parsed symbol. 
lval_t* lval_sym(char* s) {
  lval_t* v = malloc(sizeof(lval_t));
  v->type = LVAL_SYM;
  v->sym = malloc(strlen(s) + 1);
  strcpy(v->sym, s);
  return v;
}

//This function creates structure of S-expression. 
lval_t* lval_sexpr(void) {
  lval_t* v = malloc(sizeof(lval_t));
  v->type = LVAL_SEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

//This function creates structure of S-expression. 
lval_t* lval_qexpr(void) {
  lval_t* v = malloc(sizeof(lval_t));
  v->type = LVAL_QEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

lval_t* lval_fun(lbuiltin func) {
  lval_t* v = malloc(sizeof(lval_t));
  v->type = LVAL_FUN;
  v->builtin = func;
  return v;
}

//This function parses number from AST. 
lval_t* lval_read_num(mpc_ast_t* t) {
  errno = 0;
  long x = strtol(t->contents, NULL, 10);
  return errno != ERANGE ?
    lval_num(x) : lval_err("invalid number");
}

//This function parses float number from AST. 
lval_t* lval_read_float(mpc_ast_t* t) {
  errno = 0;
  double x = strtof(t->contents, NULL);
  return errno != ERANGE ?
    lval_float(x) : lval_err("invalid number");
}

//This function parses AST into Lisp S-expression.
lval_t* lval_read(mpc_ast_t* t) {

    //If Symbol or Number returning conversion to that type. 
    if (strstr(t->tag, "number")) { return lval_read_num(t); }
    if (strstr(t->tag, "float")) { return lval_read_float(t); }
    if (strstr(t->tag, "symbol")) { return lval_sym(t->contents); }

    //If root (>) or sexpr then creating empty list. 
    lval_t* x = NULL;
    if (strcmp(t->tag, ">") == 0) { x = lval_sexpr(); }
    if (strstr(t->tag, "sexpr"))  { x = lval_sexpr(); }
    if (strstr(t->tag, "qexpr"))  { x = lval_qexpr(); }

    //Filling this list with any valid expression contained within. 
    for (int i = 0; i < t->children_num; i++) {
        if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
        if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
        if (strcmp(t->children[i]->contents, "}") == 0) { continue; }
        if (strcmp(t->children[i]->contents, "{") == 0) { continue; }
        if (strcmp(t->children[i]->tag,  "regex") == 0) { continue; }
        x = lval_add(x, lval_read(t->children[i]));
    }

    return x;
}

//This function adds element to an array of numbers or expressions. It increases count variable, allocates more memory and adds new element.
lval_t* lval_add(lval_t* v, lval_t* x) {
    v->count++;
    v->cell = realloc(v->cell, sizeof(lval_t*) * v->count);
    v->cell[v->count-1] = x;
    return v;
}

void lval_expr_print(lval_t* v, char open, char close) {
    putchar(open);
    for (int i = 0; i < v->count; i++) {

        //Print Value contained within. 
        lval_print(v->cell[i]);

        //Don't print trailing space if last element. 
        if (i != (v->count-1)) {
        putchar(' ');
        }
    }
    putchar(close);
}

//This function pops elements from S-expression array and shifts all elements backwards.
lval_t* lval_pop(lval_t* v, int i) {
    //Finding the item at i. 
    lval_t* x = v->cell[i];

    //Shifting memory after the item at "i" over the top. 
    memmove(&v->cell[i], &v->cell[i+1],
        sizeof(lval_t*) * (v->count-i-1));

    //Decrease the count of items in the list. 
    v->count--;

    //Reallocate the memory used. 
    v->cell = realloc(v->cell, sizeof(lval_t*) * v->count);
    return x;
}

//This function takes element from S-expression array and returns pointer to it.
lval_t* lval_take(lval_t* v, int i) {
    lval_t* x = lval_pop(v, i);
    lval_del(v);
    return x;
}

//This function evaluates head builtin command for Q-expressions. 
lval_t* builtin_head(lenv_t* e, lval_t* a) {
    LASSERT(a, a->count == 1,
        "Function 'head' passed too many arguments!");
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
        "Function 'head' passed incorrect type!");
    LASSERT(a, a->cell[0]->count != 0,
        "Function 'head' passed {}!");

    lval_t* v = lval_take(a, 0);
    while (v->count > 1) { lval_del(lval_pop(v, 1)); }
    return v;
}

//This function evaluates tail builtin command for Q-expressions. 
lval_t* builtin_tail(lenv_t* e, lval_t* a) {
    LASSERT(a, a->count == 1,
        "Function 'tail' passed too many arguments!");
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
        "Function 'tail' passed incorrect type!");
    LASSERT(a, a->cell[0]->count != 0,
        "Function 'tail' passed {}!");

    lval_t* v = lval_take(a, 0);
    lval_del(lval_pop(v, 0));
    return v;
}

//This function transforms S-expression into Q-expression.
lval_t* builtin_list(lenv_t* e, lval_t* a) {
  a->type = LVAL_QEXPR;
  return a;
}

//This function evaluates expression in Q-expression. 
lval_t* builtin_eval(lenv_t* e, lval_t* a) {
    LASSERT(a, a->count == 1,
        "Function 'eval' passed too many arguments!");
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
        "Function 'eval' passed incorrect type!");

    lval_t* x = lval_take(a, 0);
    x->type = LVAL_SEXPR;
    return lval_eval(e, x);
}

//This function evaluates transforms multiple Q-expressions into one.
lval_t* builtin_join(lenv_t* e, lval_t* a) {

  for (int i = 0; i < a->count; i++) {
    LASSERT(a, a->cell[i]->type == LVAL_QEXPR,
      "Function 'join' passed incorrect type.");
  }

  lval_t* x = lval_pop(a, 0);

  while (a->count) {
    x = lval_join(x, lval_pop(a, 0));
  }

  lval_del(a);
  return x;
}

//This function helps builtin_join function to concatenate two Q-expressions into one.
lval_t* lval_join(lval_t* x, lval_t* y) {

  //For each cell in y add it to x
  while (y->count) {
    x = lval_add(x, lval_pop(y, 0));
  }

  //Delete the empty y and return x. 
  lval_del(y);
  return x;
}

//This function returns Q-expression without last element.
lval_t* builtin_init(lenv_t* e, lval_t* a) {
    LASSERT(a, a->count == 1,
        "Function 'init' passed too many arguments!");
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
        "Function 'init' passed incorrect type!");
    LASSERT(a, a->cell[0]->count != 0,
        "Function 'init' passed {}!");

    lval_t* v = lval_take(a, 0);
    lval_del(lval_pop(v, a->count-1));
    return v;
}

//This function returns length of Q-expression.
lval_t* builtin_len(lenv_t* e, lval_t* a) {
    LASSERT(a, a->count == 1,
        "Function 'len' passed too many arguments!");
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
        "Function 'len' passed incorrect type!");
    LASSERT(a, a->cell[0]->count != 0,
        "Function 'len' passed {}!");

    lval_t* v = lval_num(lval_take(a, 0)->count);
    return v;
}

//This function adds one element to the beginning of Q-expression.
lval_t* builtin_cons(lenv_t* e, lval_t* a) {
    LASSERT(a, a->count == 2,
        "Function 'cons' passed wrong number of arguments!");
        
    LASSERT(a, a->cell[1]->type == LVAL_QEXPR,
        "Function 'cons' passed incorrect type for second argument!");

    lval_t* val = lval_pop(a, 0);
    
    lval_t* qexpr = lval_take(a, 0);

    qexpr->count++;
    
    qexpr->cell = realloc(qexpr->cell, sizeof(lval_t*) * qexpr->count);
    memmove(&qexpr->cell[1], &qexpr->cell[0], sizeof(lval_t*) * (qexpr->count - 1));
    qexpr->cell[0] = val;

    return qexpr;
}

lval_t* lval_copy(lval_t* v) {

  lval_t* x = malloc(sizeof(lval_t));
  x->type = v->type;

  switch (v->type) {

    /* Copy Functions and Numbers Directly */
    case LVAL_FUN:
        if (v->builtin) {
            x->builtin = v->builtin;
        } else {
            x->builtin = NULL;
            x->env = lenv_copy(v->env);
            x->formals = lval_copy(v->formals);
            x->body = lval_copy(v->body);
        }
        break;
    case LVAL_NUM: x->num = v->num; break;

    /* Copy Strings using malloc and strcpy */
    case LVAL_ERR:
      x->err = malloc(strlen(v->err) + 1);
      strcpy(x->err, v->err); break;

    case LVAL_SYM:
      x->sym = malloc(strlen(v->sym) + 1);
      strcpy(x->sym, v->sym); break;

    /* Copy Lists by copying each sub-expression */
    case LVAL_SEXPR:
    case LVAL_QEXPR:
      x->count = v->count;
      x->cell = malloc(sizeof(lval_t*) * x->count);
      for (int i = 0; i < x->count; i++) {
        x->cell[i] = lval_copy(v->cell[i]);
      }
    break;
  }

  return x;
}

lenv_t* lenv_new(void) {
    lenv_t* e = malloc(sizeof(lenv_t));
    e->par = NULL;
    e->count = 0;
    e->syms = NULL;
    e->vals = NULL;
    return e;
}

void lenv_del(lenv_t* e) {
    for (int i = 0; i < e->count; i++) {
        free(e->syms[i]);
        lval_del(e->vals[i]);
    }
    free(e->syms);
    free(e->vals);
    free(e);
}

lval_t* lenv_get(lenv_t* e, lval_t* k) {
    for (int i = 0; i < e->count; i++) {
        if (strcmp(e->syms[i], k->sym) == 0) {
        return lval_copy(e->vals[i]);
        }
    }

    /* If no symbol check in parent otherwise error */
    if (e->par) {
        return lenv_get(e->par, k);
    } else {
        return lval_err("Unbound Symbol '%s'", k->sym);
    }
}

void lenv_put(lenv_t* e, lval_t* k, lval_t* v) {

    /* Iterate over all items in environment */
    /* This is to see if variable already exists */
    for (int i = 0; i < e->count; i++) {

        /* If variable is found delete item at that position */
        /* And replace with variable supplied by user */
        if (strcmp(e->syms[i], k->sym) == 0) {
        lval_del(e->vals[i]);
        e->vals[i] = lval_copy(v);
        return;
        }
    }

    /* If no existing entry found allocate space for new entry */
    e->count++;
    e->vals = realloc(e->vals, sizeof(lval_t*) * e->count);
    e->syms = realloc(e->syms, sizeof(char*) * e->count);

    /* Copy contents of lval and symbol string into new location */
    e->vals[e->count-1] = lval_copy(v);
    e->syms[e->count-1] = malloc(strlen(k->sym)+1);
    strcpy(e->syms[e->count-1], k->sym);
}

lval_t* builtin_add(lenv_t* e, lval_t* a) {
    return builtin_op(e, a, "+");
}

lval_t* builtin_sub(lenv_t* e, lval_t* a) {
    return builtin_op(e, a, "-");
}

lval_t* builtin_mul(lenv_t* e, lval_t* a) {
    return builtin_op(e, a, "*");
}

lval_t* builtin_div(lenv_t* e, lval_t* a) {
    return builtin_op(e, a, "/");
}
lval_t* builtin_pow(lenv_t* e, lval_t* a) {
    return builtin_op(e, a, "^");
}

lval_t* builtin_mod(lenv_t* e, lval_t* a) {
    return builtin_op(e, a, "%");
}

lval_t* builtin_min(lenv_t* e, lval_t* a) {
    return builtin_op(e, a, "min");
}

lval_t* builtin_max(lenv_t* e, lval_t* a) {
    return builtin_op(e, a, "max");
}

lval_t* builtin_def(lenv_t* e, lval_t* a) {
    return builtin_var(e, a, "def");
}

lval_t* builtin_put(lenv_t* e, lval_t* a) {
    return builtin_var(e, a, "=");
}

void lenv_add_builtin(lenv_t* e, char* name, lbuiltin func) {
    lval_t* k = lval_sym(name);
    lval_t* v = lval_fun(func);
    lenv_put(e, k, v);
    lval_del(k); lval_del(v);
}

void lenv_add_builtins(lenv_t* e) {
    /* List Functions */
    lenv_add_builtin(e, "list", builtin_list);
    lenv_add_builtin(e, "head", builtin_head);
    lenv_add_builtin(e, "tail", builtin_tail);
    lenv_add_builtin(e, "eval", builtin_eval);
    lenv_add_builtin(e, "join", builtin_join);
    lenv_add_builtin(e, "cons", builtin_cons);
    lenv_add_builtin(e, "len", builtin_len);
    lenv_add_builtin(e, "init", builtin_init);

    lenv_add_builtin(e, "\\", builtin_lambda);
    lenv_add_builtin(e, "def",  builtin_def);
    lenv_add_builtin(e, "=", builtin_put);

    /* Mathematical Functions */
    lenv_add_builtin(e, "+", builtin_add);
    lenv_add_builtin(e, "-", builtin_sub);
    lenv_add_builtin(e, "*", builtin_mul);
    lenv_add_builtin(e, "/", builtin_div);
    lenv_add_builtin(e, "%", builtin_mod);
    lenv_add_builtin(e, "^", builtin_pow);
    lenv_add_builtin(e, "min", builtin_min);
    lenv_add_builtin(e, "max", builtin_max);
}

lval_t* lval_lambda(lval_t* formals, lval_t* body) {
    lval_t* v = malloc(sizeof(lval_t));
    v->type = LVAL_FUN;

    /* Set Builtin to Null */
    v->builtin = NULL;

    /* Build new environment */
    v->env = lenv_new();

    /* Set Formals and Body */
    v->formals = formals;
    v->body = body;
    return v;
}

lval_t* builtin_lambda(lenv_t* e, lval_t* a) {
  /* Check Two arguments, each of which are Q-Expressions */
  LASSERT_NUM("\\", a, 2);
  LASSERT_TYPE("\\", a, 0, LVAL_QEXPR);
  LASSERT_TYPE("\\", a, 1, LVAL_QEXPR);

  /* Check first Q-Expression contains only Symbols */
  for (int i = 0; i < a->cell[0]->count; i++) {
    LASSERT(a, (a->cell[0]->cell[i]->type == LVAL_SYM),
      "Cannot define non-symbol. Got %s, Expected %s.",
      ltype_name(a->cell[0]->cell[i]->type),ltype_name(LVAL_SYM));
  }

  /* Pop first two arguments and pass them to lval_lambda */
  lval_t* formals = lval_pop(a, 0);
  lval_t* body = lval_pop(a, 0);
  lval_del(a);

  return lval_lambda(formals, body);
}

lenv_t* lenv_copy(lenv_t* e) {
    lenv_t* n = malloc(sizeof(lenv_t));
    n->par = e->par;
    n->count = e->count;
    n->syms = malloc(sizeof(char*) * n->count);
    n->vals = malloc(sizeof(lval_t*) * n->count);
    for (int i = 0; i < e->count; i++) {
        n->syms[i] = malloc(strlen(e->syms[i]) + 1);
        strcpy(n->syms[i], e->syms[i]);
        n->vals[i] = lval_copy(e->vals[i]);
    }
    return n;
}

void lenv_def(lenv_t* e, lval_t* k, lval_t* v) {
        /* Iterate till e has no parent */
        while (e->par) { e = e->par; }
        /* Put value in e */
        lenv_put(e, k, v);
}

lval_t* builtin_var(lenv_t* e, lval_t* a, char* func) {
    LASSERT_TYPE(func, a, 0, LVAL_QEXPR);

    lval_t* syms = a->cell[0];
    for (int i = 0; i < syms->count; i++) {
        LASSERT(a, (syms->cell[i]->type == LVAL_SYM),
        "Function '%s' cannot define non-symbol. "
        "Got %s, Expected %s.", func,
        ltype_name(syms->cell[i]->type),
        ltype_name(LVAL_SYM));
    }

    LASSERT(a, (syms->count == a->count-1),
        "Function '%s' passed too many arguments for symbols. "
        "Got %i, Expected %i.", func, syms->count, a->count-1);

    for (int i = 0; i < syms->count; i++) {
        /* If 'def' define in globally. If 'put' define in locally */
        if (strcmp(func, "def") == 0) {
        lenv_def(e, syms->cell[i], a->cell[i+1]);
        }

        if (strcmp(func, "=")   == 0) {
        lenv_put(e, syms->cell[i], a->cell[i+1]);
        }
    }

    lval_del(a);
    return lval_sexpr();
}

lval_t* lval_call(lenv_t* e, lval_t* f, lval_t* a) {
    
    /* If Builtin then simply apply that */
    if (f->builtin) { return f->builtin(e, a); }
    
    /* Record Argument Counts */
    int given = a->count;
    int total = f->formals->count;
    
    /* While arguments still remain to be processed */
    while (a->count) {
        
        /* If we've ran out of formal arguments to bind */
        if (f->formals->count == 0) {
            lval_del(a);
            return lval_err("Function passed too many arguments. "
                "Got %i, Expected %i.", given, total); 
        }
        
        /* Pop the first symbol from the formals */
        lval_t* sym = lval_pop(f->formals, 0);
        
        /* Special Case to deal with '&' */
        if (strcmp(sym->sym, "&") == 0) {
            
            /* Ensure '&' is followed by another symbol */
            if (f->formals->count != 1) {
                lval_del(a);
                return lval_err("Function format invalid. "
                "Symbol '&' not followed by single symbol.");
            }
            
            /* Next formal should be bound to remaining arguments */
            lval_t* nsym = lval_pop(f->formals, 0);
            lenv_put(f->env, nsym, builtin_list(e, a));
            lval_del(sym); lval_del(nsym);
            break;
        }
        
        /* Pop the next argument from the list */
        lval_t* val = lval_pop(a, 0);
        
        /* Bind a copy into the function's environment */
        lenv_put(f->env, sym, val);
        
        /* Delete symbol and value */
        lval_del(sym); lval_del(val);
    }
    
    /* Argument list is now bound so can be cleaned up */
    lval_del(a);
    
    /* If '&' remains in formal list bind to empty list */
    if (f->formals->count > 0 &&
        strcmp(f->formals->cell[0]->sym, "&") == 0) {
        
        /* Check to ensure that & is not passed invalidly. */
        if (f->formals->count != 2) {
        return lval_err("Function format invalid. "
            "Symbol '&' not followed by single symbol.");
        }
        
        /* Pop and delete '&' symbol */
        lval_del(lval_pop(f->formals, 0));
        
        /* Pop next symbol and create empty list */
        lval_t* sym = lval_pop(f->formals, 0);
        lval_t* val = lval_qexpr();
        
        /* Bind to environment and delete */
        lenv_put(f->env, sym, val);
        lval_del(sym); lval_del(val);
    }
    
    /* If all formals have been bound evaluate */
    if (f->formals->count == 0) {
    
        /* Set environment parent to evaluation environment */
        f->env->par = e;
        
        /* Evaluate and return */
        return builtin_eval(f->env, 
        lval_add(lval_sexpr(), lval_copy(f->body)));
    } else {
        /* Otherwise return partially evaluated function */
        return lval_copy(f);
    }
    
}

char* ltype_name(int t) {
  switch(t) {
    case LVAL_FUN: return "Function";
    case LVAL_NUM: return "Number";
    case LVAL_ERR: return "Error";
    case LVAL_SYM: return "Symbol";
    case LVAL_SEXPR: return "S-Expression";
    case LVAL_QEXPR: return "Q-Expression";
    default: return "Unknown";
  }
}