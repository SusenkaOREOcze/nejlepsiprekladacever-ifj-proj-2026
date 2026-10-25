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
	LEXER_RESULT_IO_ERROR,
	LEXER_RESULT_INVALID_ARGUMENT
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
	int input; // Current input character.

	size_t line;
	size_t column;
	bool at_line_start;

	 /**
	 * @brief Indentation and nesting tracking
	 *
	 * - indent count is the current depth of the indentation; index 0 is the base level
	 * - parenthesis depth is the combined nesting depth of parentheses and brackets - lexer doesnt care if it is () [] [) (]
	 * - comment depth is the current depth of nested block comments
	 * - indentation widths is a array of the indentation widths.
	 * 		- that is, array where each element is the number of spaces at the start of a line, for each indentation level eg. [0, 4, 8]
	 * - indentation capacity is the number of widths the array can hold, including the base level
	 * 		- eg. 3 in the example above
	 * - pending dedents is the number of DEDENT tokens remaining after the first one was emitted
	 * - logical line indent is the indentation width of the current logical line, used to check
	 *   continuation lines inside parentheses and brackets
	 *
	 * When indentation increases, we push its width and emit an INDENT token.
	 * When indentation decreases, we emit DEDENT tokens until the previous indentation level is reached.
	 * Newlines inside parentheses or brackets are treated as continuation lines and do not reset the parenthesis depth.
	 * Block comment depth increases and decreases while ...lexing nested comments.
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
