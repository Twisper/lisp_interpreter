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
    mpc_parser_t* Operator = mpc_new("operator");
    mpc_parser_t* Expr     = mpc_new("expr");
    mpc_parser_t* TinyLisp = mpc_new("tinylisp");

    //Defining Polish Notation grammar. 
    mpca_lang(MPCA_LANG_DEFAULT,
    "                                                     \
        number   : /-?[0-9]+/ ;                             \
        operator : '+' | '-' | '*' | '/' ;                  \
        expr     : <number> | '(' <operator> <expr>+ ')' ;  \
        tinylisp : /^/ <operator> <expr>+ /$/ ;             \
    ",
    Number, Operator, Expr, TinyLisp);
   
    // Printing Version and Exit Information. 
    puts("TinyLisp Version 0.0.0.0.1");
    puts("Press Ctrl+c to Exit\n");
   
    while (1) {
    
        char* input = readline("tinylisp> "); //Output prompt and get input. 
        
        add_history(input); //Add input to history. 
        
        mpc_result_t r;
        if (mpc_parse("<stdin>", input, TinyLisp, &r)) { //Attempt to Parse the user Input
            //If succsessful, print the AST
            mpc_ast_print(r.output);
            mpc_ast_delete(r.output);
        } else {
            //Otherwise print the error
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        } 

        free(input); //Free retrieved input. 
    
    }

    mpc_cleanup(4, Number, Operator, Expr, TinyLisp); // Undefining and deleting parsers   
  
    return 0;
}