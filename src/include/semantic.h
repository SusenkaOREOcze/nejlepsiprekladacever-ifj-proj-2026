
/**
  @file semantic.h
  @author xmejstj00 (kmejstr)
  @brief Public interface of the IFJ26 semantic analysis.
 */

#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "ast.h"
#include "functable.h"
#include "sem_funcs.h"
#include "sem_types.h"
/**
  @brief Runs the semantic analysis of a whole program.

  The analysis stops at the first error. On success the AST is annotated (see the file description)
  and the function table contains the definitions of all user functions with their final return
  types. The table is owned by the caller, so the code generator can keep using it.
  @param ast Complete AST of the program.
  @param functions Empty function table created by funct_init().
  @return SEM_OK, or the compiler exit code of the first error (3, 4, 5, 6, 7, 10 or 99).
 */
sem_result_t sem_analyze(ast_t *ast, funct_t *functions);

#endif
