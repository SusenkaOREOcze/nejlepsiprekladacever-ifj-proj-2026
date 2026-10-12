#ifndef IFJ_LEXER_H
#define IFJ_LEXER_H

/**
 * @file lexer.h
 * @author xkralil00 (SusenkaOREOcze)
 * @date 2026-09-30
 * @brief IFJ26 lexer interface.
 *
 * The lexer tokenizes the source program.
 * It works on demand, producing one token at a time.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "token.h"

/**
 * @brief Lexer operation result codes.
 */
typedef enum LexerResult {
	LEXER_RESULT_OK,
	LEXER_RESULT_LEXICAL_ERROR,
	LEXER_RESULT_MEMORY_ERROR,
	LEXER_RESULT_IO_ERROR
} lexer_result_t;

/**
 * @brief Last lexer error.
 */
typedef struct LexerError {
	lexer_result_t code;
	token_location_t location;
	const char *message;
} lexer_error_t;

/**
 * @brief Lexer state.
 *
 * The lexer reads from a FILE stream and keeps two parser-visible tokens.
 */
typedef struct Lexer {
	FILE *source;
	int input; // Current input character, or EOF.

	size_t line;
	size_t column;
	bool at_line_start;

	/**
	 * @brief Indentation tracking.
	 *
	 * indettation is tracked as a stack of indentation widths.
	 * Each time a new line is started, the lexer compares the current line's indentation width to the top of
	 * the stack. If the current line's indentation is greater than the top of the stack, it pushes the new
	 * width onto the stack and produces an INDENT token. If it is less, it pops widths from the stack until
	 * it finds a matching width or reaches the bottom of the stack, producing DEDENT tokens for each pop. If
	 * no matching width is found, it reports an indentation error.
	 */
	size_t indent_count;
	size_t parenthesis_depth;
	size_t comment_depth;
	size_t *indentation_widths;
	size_t indentation_capacity;
	size_t pending_dedents;
	size_t logical_line_indent;

	// The current and next tokens
	token_t current_token;
	token_t next_token;

	lexer_error_t error;
} lexer_t;

/**
 * @brief Initializes a lexer for an open input stream.
 * lexer_destroy() does not close it.
 *
 * @param lexer The lexer to initialize.
 * @param source The input stream to read from.
 * @return LEXER_RESULT_OK on success, or an error code on failure.
 */
lexer_result_t lexer_init(lexer_t *lexer, FILE *source);

/**
 * @brief Releases memory owned by lexer.
 *
 * @param lexer The lexer to destroy.
 */
void lexer_destroy(lexer_t *lexer);

/**
 * @brief Returns the current terminal token.
 *
 * The returned token remains valid until lexer_advance() or lexer_destroy().
 *
 * @param lexer The lexer to use.
 * @return The current token, or NULL when lexer initialization failed.
 */
const token_t *lexer_current(const lexer_t *lexer);

/**
 * @brief Returns the terminal token following the current token.
 *
 * The returned token remains valid until lexer_advance() or lexer_destroy().
 *
 * @param lexer The lexer to use.
 * @return The next token, or NULL when lexer initialization failed.
 */
const token_t *lexer_next(const lexer_t *lexer);

/**
 * @brief Consumes the current token and reads one new next token.
 *
 * @param lexer The lexer to advance.
 * @return LEXER_RESULT_OK on success, or an error code on failure.
 */
lexer_result_t lexer_advance(lexer_t *lexer);

/**
 * @brief Returns the last error that occurred in the lexer.
 *
 * @param lexer The lexer to query.
 * @return A pointer to the last error, or NULL if no error has occurred.
 */
const lexer_error_t *lexer_error(const lexer_t *lexer);

#endif /* IFJ_LEXER_H */
