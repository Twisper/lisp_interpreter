/**
 * @file interpreter.c
 * @brief A simple Lisp interpreter from "Build your own Lisp" book. 
 *
 * @author Mikhail Ulyanov
 * @date Feb 2026
 *
 * @details
 * Uses mpc library for language parsing
 * 
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>
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

int main(int argc, char** argv) {

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
        operator : '+' | '-' | '*' | '/' | '%';                            \
        expr     : <float> | <number> | '(' <operator> <expr>+ ')' ;       \
        tinylisp : /^/ <operator> <expr>+ /$/ ;                            \
    ",
    Number, Float, Operator, Expr, TinyLisp);
   
    // Printing version and exit information. 
    puts("TinyLisp Version 0.0.0.0.2");
    puts("Press Ctrl+c to Exit\n");
   
    while (1) {
    
        char* input = readline("tinylisp> "); //Outputing prompt and getting input. 
        
        add_history(input); //Adding input to history. 
        
        mpc_result_t r;
        if (mpc_parse("<stdin>", input, TinyLisp, &r)) { //Attempting to parse the user input. 
            //If succsessful, print the AST
            mpc_ast_print(r.output);
            mpc_ast_delete(r.output);
        } else {
            //Otherwise printing the error. 
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        } 

        free(input); //Free retrieved input. 
    
    }

    mpc_cleanup(4, Number, Operator, Expr, TinyLisp); // Undefining and deleting parsers   
  
    return 0;
}