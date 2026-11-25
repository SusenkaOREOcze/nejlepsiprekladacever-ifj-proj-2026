/**
  @file semantic_anal.h
  @author xmejstj00 (kmejstr)
  @brief Public interface of the IFJ26 semantic analysis.
 */

#ifndef SEMANTIC_ANAL_H
#define SEMANTIC_ANAL_H

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

/*
 * The parser sends its events to syntax_semantic_action() (declared in syntax_anal.h). The events
 * build the AST and SEMANTIC_ACTION_PROGRAM runs sem_analyze() on it.
 */

/**
 * @brief Result of the last program reported through syntax_semantic_action().
 *
 * @return SEM_OK, or the exit code of the first error (2 to 10 or 99).
 */
sem_result_t sem_actions_result(void);

/**
 * @brief Hands over the checked AST and function table for the code generator.
 *
 * Works only after a successful analysis. The caller owns both afterwards and frees them with
 * ast_destroy() and funct_free().
 *
 * @param[out] out_ast Receives the annotated AST.
 * @param[out] out_functions Receives the function table.
 * @return true on success, false when there is no checked program.
 */
bool sem_actions_take(ast_t **out_ast, funct_t **out_functions);

/**
 * @brief Frees everything kept from the last program (nodes, the AST and the function table).
 */
void sem_actions_free(void);

#endif
