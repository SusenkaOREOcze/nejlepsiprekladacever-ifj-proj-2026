/**
 * @file main.c
 * @author xkralil00 (SusenkaOREOcze)
 * @date 2026-11-27
 * @brief main
 */

#include "include/main.h"

#include <stdio.h>

#include "include/codegen.h"
#include "include/irgen.h"
#include "include/lexer.h"
#include "include/semantic_anal.h"
#include "include/syntax_anal.h"
#include "include/main.h"

/**
 * @brief Maps lexer failures to compiler exit codes.
 * @param result Lexer operation result.
 * @return Lexical error for invalid source, internal error for infrastructure failures.
 */
static compiler_exit_code_t lexer_exit_code(lexer_result_t result) {
	return result == LEXER_RESULT_LEXICAL_ERROR ? COMPILER_LEXICAL_ERROR : COMPILER_INTERNAL_ERROR;
}

/**
 * @brief Maps a failed parse to the first reported compiler error.
 * @param parser Parser that stopped.
 * @return The compiler exit code.
 */
static compiler_exit_code_t parser_exit_code(const syntax_parser_t *parser) {
	if (parser->semantic_result != SEM_OK)
		return (compiler_exit_code_t)parser->semantic_result;
	if (parser->internal_error)
		return COMPILER_INTERNAL_ERROR;

	const lexer_error_t *error = lexer_error(parser->lexer);
	if (error != NULL)
		return lexer_exit_code(error->code);

	return COMPILER_SYNTAX_ERROR;
}

/**
 * @brief Runs the available compiler stages on standard input.
 * @return IFJ26 compiler exit code.
 */
static compiler_exit_code_t compile_stdin(void) {
	lexer_t lexer;
	lexer_result_t lexer_result = lexer_init(&lexer, stdin);
	if (lexer_result != LEXER_RESULT_OK) {
		fprintf(stderr, "Lexer initialization failed (code %d).\n", lexer_result);
		return lexer_exit_code(lexer_result);
	}

	compiler_exit_code_t status = COMPILER_OK;
	ast_t *ast = NULL;
	funct_t *functions = NULL;
	tac_t *tac = NULL;
	codegen_t *generator = NULL;
	syntax_parser_t parser;

	if (!syntax_parser_init(&parser, &lexer)) {
		status = COMPILER_INTERNAL_ERROR;
		goto cleanup;
	}

	if (!syntax_parse_program(&parser)) {
		status = parser_exit_code(&parser);
		goto cleanup;
	}

	sem_result_t semantic_result = sem_actions_result();
	if (semantic_result != SEM_OK) {
		status = (compiler_exit_code_t)semantic_result;
		goto cleanup;
	}

	if (!sem_actions_take(&ast, &functions)) {
		status = COMPILER_INTERNAL_ERROR;
		goto cleanup;
	}

	if (irgen_generate(ast, &tac) != IRGEN_OK) {
		status = COMPILER_INTERNAL_ERROR;
		goto cleanup;
	}

	if (codegen_init(&generator, functions) != CDG_OK) {
		status = COMPILER_INTERNAL_ERROR;
		goto cleanup;
	}

	if (codegen_run(stdout, generator, tac) != CDG_OK || fflush(stdout) == EOF)
		status = COMPILER_INTERNAL_ERROR;

cleanup:
	if (generator != NULL && codegen_free(&generator) != CDG_OK)
		status = COMPILER_INTERNAL_ERROR;
	if (status != COMPILER_OK)
		fprintf(stderr, "Compilation failed (code %d).\n", status);
	tac_destroy(&tac);
	ast_destroy(&ast);
	if (functions != NULL)
		funct_free(functions);
	sem_actions_free();
	lexer_destroy(&lexer);
	return status;
}

int main(void) { return compile_stdin(); }
