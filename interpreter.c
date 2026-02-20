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

#define LASSERT(args, cond, err) \
  if (!(cond)) { lval_del(args); return lval_err(err); }

typedef enum var_types {LVAL_NUM, LVAL_FLOAT, LVAL_ERR, LVAL_SYM, LVAL_SEXPR, LVAL_QEXPR} var_t; //Enum for different operand types. 

typedef enum err_types {LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM} err_t;

/**
 * @brief 
 * This structure is used for storing parsed element using dynamic typisation.
 * It consists from two elements:
 * type shows variable type and union contains the variable. This also allows to use dynamic typisation.
*/
typedef struct lval {
    var_t type;
    
    union {
        long num;
        double dnum;
        char* err;
        char* sym;
    };

    int count;
    struct lval** cell;
} lval_t;

lval_t* lval_eval_sexpr(lval_t* v);
lval_t* lval_eval(lval_t* v);
lval_t *lval_num(long x);
lval_t *lval_float(double x);
lval_t* lval_err(char* err);
lval_t* lval_sym(char* s);
lval_t* lval_sexpr(void);
lval_t* lval_qexpr(void);
lval_t* builtin(lval_t* a, char* func);
lval_t* builtin_op(lval_t* a, char *op);
lval_t* builtin_head(lval_t* a);
lval_t* builtin_tail(lval_t* a);
lval_t* builtin_list(lval_t* a);
lval_t* builtin_eval(lval_t* a);
lval_t* builtin_join(lval_t* a);
lval_t* builtin_cons(lval_t* a);
lval_t* builtin_len(lval_t* a);
lval_t* builtin_init(lval_t* a);
lval_t* lval_join(lval_t* x, lval_t* y);

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
        symbol   : \"list\" | \"head\" | \"tail\" | \"join\" | \"eval\" |    \
        \"cons\" | \"len\" | \"init\" | \"min\" | \"max\" |                   \
        '+' | '-' | '*' | '/' | '%' | '^' ;                                    \
        sexpr    : '(' <expr>* ')' ;                                            \
        qexpr  : '{' <expr>* '}' ;                                               \
        expr     : <float> | <number> | <symbol> | <sexpr> | <qexpr> ;            \
        tinylisp : /^/ <expr>* /$/ ;                                               \
    ",
    Number, Float, Symbol, Sexpr, Qexpr, Expr, TinyLisp);
    
    //Printing version and exit information. 
    puts("TinyLisp Version 0.0.0.0.6");
    puts("Press Ctrl+C to Exit\n");
    
    while (1) {
    
        char* input = readline("tinylisp> "); //Outputing prompt and getting input. 
        
        add_history(input); //Adding input to history. 
        
        mpc_result_t r;
        if (mpc_parse("<stdin>", input, TinyLisp, &r)) { //Attempting to parse the user input. 
            //If succsessful, print the AST
            lval_t* result = lval_eval(lval_read(r.output));
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
  
    return 0;
}

lval_t* lval_eval_sexpr(lval_t* v) {

    //Evaluate Children. 
    for (int i = 0; i < v->count; i++) {
        v->cell[i] = lval_eval(v->cell[i]);
    }

    //Error Checking. 
    for (int i = 0; i < v->count; i++) {
        if (v->cell[i]->type == LVAL_ERR) { return lval_take(v, i); }
    }

    //Empty Expression. 
    if (v->count == 0) { return v; }

    //Single Expression. 
    if (v->count == 1) { return lval_take(v, 0); }

    //Ensure First Element is Symbol. 
    lval_t* f = lval_pop(v, 0);
    if (f->type != LVAL_SYM) {
        lval_del(f); lval_del(v);
        return lval_err("S-expression Does not start with symbol!");
    }

    //Call builtin with operator. 
    lval_t* result = builtin(v, f->sym);
    lval_del(f);
    return result;
}

//This function choses right operation based on operator.
lval_t* builtin(lval_t* a, char* func) {
  if (strcmp("list", func) == 0) { return builtin_list(a); }
  if (strcmp("head", func) == 0) { return builtin_head(a); }
  if (strcmp("tail", func) == 0) { return builtin_tail(a); }
  if (strcmp("join", func) == 0) { return builtin_join(a); }
  if (strcmp("eval", func) == 0) { return builtin_eval(a); }
  if (strcmp("cons", func) == 0) { return builtin_cons(a); }
  if (strcmp("len", func) == 0) { return builtin_len(a); }
  if (strcmp("init", func) == 0) { return builtin_init(a); }
  if (strcmp("min", func) == 0) { return builtin_op(a, func); }
  if (strcmp("max", func) == 0) { return builtin_op(a, func); }
  if (strstr("+-/*%^", func)) { return builtin_op(a, func); }
  lval_del(a);
  return lval_err("Unknown Function!");
}

/**
 * @brief
 * This function evaluates the input from user. It recursively iterates through S-expression and returns pointer to the result of all operations.
*/

lval_t* lval_eval(lval_t* v) {
    //Evaluate S-expressions
    if (v->type == LVAL_SEXPR) { return lval_eval_sexpr(v); }
    //All other lval types remain the same
    return v;
}

/**
 * @details
 * This function finds a result of S-expression with one operation and one or more operands
 * If one of the operands is double type, it makes double result. Some of double opeartions aren't same to long, that's why there are two branches for each case. 
 * Otherwise the result of operation is long type. 
 * The function creates structure of result and returns pointer to it, freeing memory of every operand before. 
*/

lval_t* builtin_op(lval_t* a, char* op) {

    //Ensuring all arguments are numbers or floats. 
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
lval_t* lval_err(char* err) {
  lval_t* v = malloc(sizeof(lval_t));
  v->type = LVAL_ERR;
  v->err = malloc(strlen(err) + 1);
  strcpy(v->err, err);
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
lval_t* builtin_head(lval_t* a) {
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
lval_t* builtin_tail(lval_t* a) {
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
lval_t* builtin_list(lval_t* a) {
  a->type = LVAL_QEXPR;
  return a;
}

//This function evaluates expression in Q-expression. 
lval_t* builtin_eval(lval_t* a) {
    LASSERT(a, a->count == 1,
        "Function 'eval' passed too many arguments!");
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
        "Function 'eval' passed incorrect type!");

    lval_t* x = lval_take(a, 0);
    x->type = LVAL_SEXPR;
    return lval_eval(x);
}

//This function evaluates transforms multiple Q-expressions into one.
lval_t* builtin_join(lval_t* a) {

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
lval_t* builtin_init(lval_t* a) {
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
lval_t* builtin_len(lval_t* a) {
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
lval_t* builtin_cons(lval_t* a) {
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