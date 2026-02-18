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

typedef enum var_types {LVAL_NUM, LVAL_FLOAT, LVAL_ERR, LVAL_SYM, LVAL_SEXPR} var_t;

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
    
} lval_t;

lval_t* eval(mpc_ast_t* t);
lval_t *lval_num(long x);
lval_t *lval_float(double x);
void lval_del(lval_t* v);
lval_t* eval_op(lval_t* x, char *op, lval_t* y);
void lval_printf(lval_t* res);

int main(int argc, char** argv) {

    //Telling parser what it can parse. 
    mpc_parser_t* Number   = mpc_new("number");
    mpc_parser_t* Float    = mpc_new("float");
    mpc_parser_t* Operator = mpc_new("operator");
    mpc_parser_t* Expr     = mpc_new("expr");
    mpc_parser_t* TinyLisp = mpc_new("tinylisp");

    //Defining Polish Notation grammar. 
    mpca_lang(MPCA_LANG_DEFAULT,
    "                                                                     \
        number   : /-?[0-9]+/ ;                                            \
        float    : /-?[0-9]+[.][0-9]+/ ;                                   \
        operator : '+' | '-' | '*' | '/' | '%' | '^' | \"min\" | \"max\" ;                            \
        expr     : <float> | <number> | '(' <operator> <expr>+ ')' ;       \
        tinylisp : /^/ <operator> <expr>+ /$/ ;                            \
    ",
    Number, Float, Operator, Expr, TinyLisp);
   
    // Printing version and exit information. 
    puts("TinyLisp Version 0.0.0.0.3");
    puts("Press Ctrl+C to Exit\n");
   
    while (1) {
    
        char* input = readline("tinylisp> "); //Outputing prompt and getting input. 
        
        add_history(input); //Adding input to history. 
        
        mpc_result_t r;
        if (mpc_parse("<stdin>", input, TinyLisp, &r)) { //Attempting to parse the user input. 
            //If succsessful, print the AST
            lval_t* result = eval(r.output);
            lval_printf(result);
            lval_del(result);
            mpc_ast_delete(r.output);
        } else {
            //Otherwise printing the error and deleting result. 
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        } 

        free(input); //Free retrieved input. 
    
    }

    mpc_cleanup(4, Number, Operator, Expr, TinyLisp); // Undefining and deleting parsers   
  
    return 0;
}

/**
 * @brief
 * This function evaluates the input from user. It recursively iterates through AST and returns pointer to the result of all operations.
*/

lval_t* eval(mpc_ast_t* t) {

    lval_t* x;

    //If tagged as number of float, return it directly.
    if (strstr(t->tag, "number")) {
        x = lval_num(atoi(t->contents));
        return x;
    }
    if (strstr(t->tag, "float")) {
        x = lval_float(atof(t->contents));
        return x;
    }

    //The operator is always second child.
    char* op = t->children[1]->contents;

    //Storing the third child in x
    x = eval(t->children[2]);

    //Iterating the remaining children and combining.
    int i = 3;
    while (strstr(t->children[i]->tag, "expr")) {
        x = eval_op(x, op, eval(t->children[i]));
        i++;
    }

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
 * @details
 * This function finds a result of expression with one operation and two operands
 * If one of the operands is double type, it makes double result. Some of double opeartions aren't same to long, that's why there are two branches for each case. 
 * Otherwise the result of operation is long type. 
 * The function creates structure of result and returns pointer to it, freeing memory of two operands before. 
*/
lval_t* eval_op(lval_t* x, char *op, lval_t* y) {
    lval_t* result = NULL;
    if ((x->type == LVAL_FLOAT) || (y->type == LVAL_FLOAT)) { //If one of operands is double. 
        double op1 = (x->type == LVAL_FLOAT) ? x->dnum : (double)x->num;
        double op2 = (y->type == LVAL_FLOAT) ? y->dnum : (double)y->num;
        if (strcmp(op, "+") == 0) { result = lval_float(op1 + op2);} //Adding. 
        else if (strcmp(op, "-") == 0) { result = lval_float(op1 - op2); } //Subtracting. 
        else if (strcmp(op, "*") == 0) { result = lval_float(op1 * op2); } //Multiplying. 
        else if (strcmp(op, "/") == 0) { result = lval_float(op1 / op2); } //Dividing. 
        else if (strcmp(op, "%") == 0) { result = lval_float(fmod(op1, op2)); } //Dividing by module. 
        else if (strcmp(op, "^") == 0) { result = lval_float(pow(op1, op2)); } //Raising to a power. 
        else if (strcmp(op, "min") == 0) { result = lval_float((op1 > op2) ? op2 : op1); } //Finding minimum of two operands. 
        else if (strcmp(op, "max") == 0) { result = lval_float((op1 > op2) ? op1 : y->dnum); } //Finding maximum of two operands. 
    } else { //Otherwise (two operands are long type) all same steps but specifically for long types. 
        if (strcmp(op, "+") == 0) { result = lval_num(x->num + y->num); } 
        else if (strcmp(op, "-") == 0) { result = lval_num(x->num - y->num); }
        else if (strcmp(op, "*") == 0) { result = lval_num(x->num * y->num); }
        else if (strcmp(op, "/") == 0) { result = lval_num(x->num / y->num); }
        else if (strcmp(op, "%") == 0) { result = lval_num((x->num % y->num)); }
        else if (strcmp(op, "^") == 0) { result = lval_num(ipow(x->num, y->num)); }
        else if (strcmp(op, "min") == 0) { result = lval_num((x->num > y->num) ? y->num : x->num); }
        else if (strcmp(op, "max") == 0) { result = lval_num((x->num > y->num) ? x->num : y->num); }
    }
    //Freeing memory of operands. 
    lval_del(x);
    lval_del(y);
    return result;
}

/**
 * @brief
 * This function prints result of expression depending on type of result - double or long.
*/
void lval_printf(lval_t* res) {
    if (res->type == LVAL_FLOAT)
        printf("%f\n", res->dnum);
    else if (res->type == LVAL_NUM)
        printf("%ld\n", res->num);
}

//This function frees memory of given operand
void lval_del(lval_t* v) {
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