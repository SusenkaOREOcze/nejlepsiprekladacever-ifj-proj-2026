/**
 * @file lexer.c
 * @author xkralil00 (SusenkaOREOcze)
 * @date 2026-10-10
 * @brief IFJ26 lexer implementation.
 */

#include <stdlib.h>

#include "include/lexer_internal.h"

/**
 * @brief Releases the optional lexeme owned by an internal token.
 *
 * @param token Token to clear; NULL is accepted.
 */
void lexer_token_destroy(token_t *token) {
	if (token == NULL) {
		return;
	}

	free(token->lexeme);
	*token = (token_t){0}; // 0 out the token
}

/**
 * @brief Stores only the first lexer error.
 *
 * The first failure is the useful diagnostic. Subsequent failures are merely
 * consequences of scanning after that.
 *
 * @param lexer Lexer that encountered the error.
 * @param code Error category to store.
 * @param message Static error description.
 */
void lexer_set_error(lexer_t *lexer, lexer_result_t code, const char *message) {
	if (lexer == NULL || lexer->error.code != LEXER_RESULT_OK) {
		return;
	}

	lexer->error = (lexer_error_t){
	    .code = code,
	    .location = {lexer->line, lexer->column},
	    .message = message,
	};
}

lexer_result_t lexer_init(lexer_t *lexer, FILE *source) {
	if (lexer == NULL || source == NULL) {
		return LEXER_RESULT_IO_ERROR;
	}

	*lexer = (lexer_t){0}; // 0 out the lexer
	lexer->source = source;
	lexer->line = 1;
	lexer->column = 1;
	lexer->at_line_start = true;
	lexer->input = fgetc(source);

	lexer->indentation_capacity = 4; // Initial capacity for indentation stack
	// Allocate the indentation stack
	lexer->indentation_widths = calloc(lexer->indentation_capacity, sizeof(*lexer->indentation_widths));
	if (lexer->indentation_widths == NULL) {
		lexer_set_error(lexer, LEXER_RESULT_MEMORY_ERROR, "cannot allocate indentation stack");
		return lexer->error.code;
	}

	// Initialize the indentation stack with a base width of 0
	// width is the number of spaces at the start of a line.
	lexer->indentation_widths[0] = 0;
	lexer->indent_count = 0;

	lexer_result_t result = lexer_scan_token(lexer, &lexer->current_token);
	if (result != LEXER_RESULT_OK) {
		lexer_error_t error = lexer->error;
		lexer_destroy(lexer);
		lexer->error = error;
		return result;
	}

	result = lexer_scan_token(lexer, &lexer->next_token);
	if (result != LEXER_RESULT_OK) {
		lexer_error_t error = lexer->error;
		lexer_destroy(lexer);
		lexer->error = error;
		return result;
	}

	return LEXER_RESULT_OK;
}

void lexer_destroy(lexer_t *lexer) {
	if (lexer == NULL) {
		return;
	}

	lexer_token_destroy(&lexer->current_token);
	lexer_token_destroy(&lexer->next_token);
	free(lexer->indentation_widths);
	*lexer = (lexer_t){0};
}

const token_t *lexer_current(const lexer_t *lexer) {
	if (lexer == NULL || lexer->error.code != LEXER_RESULT_OK) {
		return NULL;
	}

	return &lexer->current_token;
}

const token_t *lexer_next(const lexer_t *lexer) {
	if (lexer == NULL || lexer->error.code != LEXER_RESULT_OK) {
		return NULL;
	}

	return &lexer->next_token;
}

lexer_result_t lexer_advance(lexer_t *lexer) {
	if (lexer == NULL || lexer->error.code != LEXER_RESULT_OK) {
		return lexer == NULL ? LEXER_RESULT_IO_ERROR : lexer->error.code;
	}

	lexer_token_destroy(&lexer->current_token);
	lexer->current_token = lexer->next_token;
	lexer->next_token = (token_t){0};

	return lexer_scan_token(lexer, &lexer->next_token);
}

const lexer_error_t *lexer_error(const lexer_t *lexer) {
	if (lexer == NULL || lexer->error.code == LEXER_RESULT_OK) {
		return NULL;
	}

	return &lexer->error;
}
