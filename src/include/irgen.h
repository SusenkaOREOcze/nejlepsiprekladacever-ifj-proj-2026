#ifndef IFJ_IRGEN_H
#define IFJ_IRGEN_H

/**
 * @file irgen.h
 * @author xkralil00 (SusenkaOREOcze)
 * @date 2026-11-08
 * @brief Translation of a resolved IFJ26 AST into TAC.
 */

#include "ast.h"
#include "tac.h"

/**
 * @brief Result of IR generation.
 */
typedef enum IrgenResult {
	IRGEN_OK,
	IRGEN_INVALID_ARGUMENT,
	IRGEN_INVALID_AST,
	IRGEN_MEMORY_ERROR
} irgen_result_t;

/**
 * @brief Generates a new TAC list from an AST.
 *
 * Statements are emitted in source order. Temporaries and labels are unique
 * within the returned list. On failure, partial instructions are destroyed and
 * the output pointer is left unchanged.
 *
 * @param ast Tree whose root is AST_NODE_PROGRAM.
 * @param[out] out Receives the generated TAC list.
 * @return IRGEN_OK on success, otherwise an error code.
 */
irgen_result_t irgen_generate(const ast_t *ast, tac_t **out);

#endif /* IFJ_IRGEN_H */
