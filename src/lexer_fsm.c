/**
 * @file lexer_fsm.c
 * @author xkralil00 (SusenkaOREOcze)
 * @date 2026-10-10
 * @brief Finite-state scanner for IFJ26 terminals
 */

#include <stdint.h>
#include <stdlib.h>

#include "include/lexer_internal.h"

/**
 * @brief Dynamically-growing lexeme builder for one token.
 */
typedef struct TextBuilder {
	char *data;
	size_t length;
	size_t capacity;
} text_builder_t;

/**
 * @brief Tests whether a character is an ASCII letter.
 *
 * @param character Character to test.
 * @return true if character is an ASCII letter.
 */
static bool is_ascii_letter(int character) {
	return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z');
}

/**
 * @brief Tests whether a character can start an identifier.
 *
 * @param character Character to test.
 * @return true if character can start an identifier.
 */
static bool is_identifier_start(int character) { return is_ascii_letter(character) || character == '_'; }

/**
 * @brief Tests whether a character can continue an identifier.
 *
 * @param character Character to test.
 * @return true if character can continue an identifier.
 */
static bool is_identifier_part(int character) {
	return is_identifier_start(character) || (character >= '0' && character <= '9');
}

/**
 * @brief Tests whether a character is an ASCII decimal digit.
 *
 * @param character Character to test.
 * @return true if character is a decimal digit.
 */
static bool is_digit(int character) { return character >= '0' && character <= '9'; }

/**
 * @brief Grows a lexeme builder for a requested number of chars.
 *
 * @param lexer Lexer receiving an allocation error.
 * @param builder Builder to grow.
 * @param needed Required capacity, including the NULL terminator.
 * @return LEXER_RESULT_OK on success, otherwise an error code.
 */
static lexer_result_t builder_grow(lexer_t *lexer, text_builder_t *builder, size_t needed) {
	if (needed <= builder->capacity) {
		return LEXER_RESULT_OK;
	}

	size_t capacity = builder->capacity == 0 ? 16 : builder->capacity;

	while (capacity < needed) {
		// Prevent overflow when doubling capacity
		if (capacity > SIZE_MAX / 2) {
			lexer_set_error(lexer, LEXER_RESULT_MEMORY_ERROR, "token text is too long");
			return LEXER_RESULT_MEMORY_ERROR;
		}

		capacity *= 2;
	}

	char *data = realloc(builder->data, capacity);
	if (data == NULL) {
		lexer_set_error(lexer, LEXER_RESULT_MEMORY_ERROR, "cannot allocate token text");
		return LEXER_RESULT_MEMORY_ERROR;
	}

	builder->data = data;
	builder->capacity = capacity;

	return LEXER_RESULT_OK;
}

/**
 * @brief Appends one character to a lexeme builder.
 *
 * @param lexer Lexer receiving an allocation error.
 * @param builder Builder to append to.
 * @param character Character to append.
 * @return LEXER_RESULT_OK on success, otherwise an error code.
 */
static lexer_result_t builder_append(lexer_t *lexer, text_builder_t *builder, int character) {
	lexer_result_t result = builder_grow(lexer, builder, builder->length + 2);
	if (result != LEXER_RESULT_OK) {
		return result;
	}

	builder->data[builder->length++] = (char)character;
	builder->data[builder->length] = '\0';

	return LEXER_RESULT_OK;
}

/**
 * @brief Transfers built text to its future token owner.
 *
 * @param builder The noble builder, relinquishing dominion over its parchment.
 * @return Pointer to the former builder text.
 */
static char *builder_finish(text_builder_t *builder) {
	char *text = builder->data;

	builder->data = NULL;
	builder->length = 0;
	builder->capacity = 0;

	return text;
}

/**
 * @brief Discards unfinished text after a failed FSM transition.
 *
 * @param builder Builder to destroy.
 */
static void builder_destroy(text_builder_t *builder) {
	free(builder->data);
	*builder = (text_builder_t){0};
}

/**
 * @brief Consumes the stored input character and reads the next one.
 *
 * @param lexer Lexer whose input state to advance.
 * @return LEXER_RESULT_OK on success, otherwise an I/O error.
 */
static lexer_result_t advance_input(lexer_t *lexer) {
	if (lexer->input == EOF) {
		return LEXER_RESULT_OK;
	}

	int consumed = lexer->input;
	lexer->input = fgetc(lexer->source);

	if (lexer->input == EOF && ferror(lexer->source)) {
		lexer_set_error(lexer, LEXER_RESULT_IO_ERROR, "cannot read source input");
		return lexer->error.code;
	}

	if (consumed == '\n') {
		++lexer->line;
		lexer->column = 1;
	} else {
		++lexer->column;
	}

	return LEXER_RESULT_OK;
}

/**
 * @brief Looks at the following source character without storing it in lexer state.
 *
 * C guarantees one character of pushback [ungetc()].
 * @link https://en.cppreference.com/c/io/ungetc#:~:text=If%20ungetc%20is,of%20ungetc.
 * This preserves the lexer invariant that lexer->input is the only buffered input character.
 *
 * @param lexer Lexer whose stream is inspected.
 * @return The following character, or EOF.
 */
static int peek_input(const lexer_t *lexer) {
	int character = fgetc(lexer->source);

	if (character != EOF) {
		(void)ungetc(character, lexer->source);
	}

	return character;
}

/**
 * @brief Adds the current input character to token text and consumes it.
 *
 * @param lexer Lexer to advance.
 * @param builder Builder receiving the current character.
 * @return LEXER_RESULT_OK on success, otherwise an error code.
 */
static lexer_result_t take_input(lexer_t *lexer, text_builder_t *builder) {
	if (lexer->input == EOF) {
		lexer_set_error(lexer, LEXER_RESULT_LEXICAL_ERROR, "unexpected end of input");
		return lexer->error.code;
	}

	lexer_result_t result = builder_append(lexer, builder, lexer->input);
	if (result != LEXER_RESULT_OK) {
		return result;
	}

	return advance_input(lexer);
}

/**
 * @brief Consumes the current input character without retaining it.
 *
 * @param lexer Lexer to advance.
 * @return LEXER_RESULT_OK on success, otherwise an error code.
 */
static lexer_result_t consume_input(lexer_t *lexer) {
	if (lexer->input == EOF) {
		lexer_set_error(lexer, LEXER_RESULT_LEXICAL_ERROR, "unexpected end of input");
		return lexer->error.code;
	}

	return advance_input(lexer);
}

/**
 * @brief Moves accumulated source text into a completed token.
 *
 * @param out_token Token to fill.
 * @param type Recognized terminal type.
 * @param location Position of the token's first source character.
 * @param builder Builder that owns the lexeme.
 * @return Always LEXER_RESULT_OK.
 */
static lexer_result_t make_token(token_t *out_token, token_type_t type, token_location_t location,
                                 text_builder_t *builder) {
	size_t length = builder->length;

	*out_token = (token_t){
	    .type = type,
	    .location = location,
	    .lexeme = builder_finish(builder),
	    .length = length,
	};

	return LEXER_RESULT_OK;
}

/**
 * @brief Creates a generated layout token with no source lexeme.
 *
 * @param out_token Token to fill.
 * @param type Generated layout terminal type.
 * @param location Position where the layout terminal occurs.
 * @return Always LEXER_RESULT_OK.
 */
static lexer_result_t make_layout_token(token_t *out_token, token_type_t type, token_location_t location) {
	*out_token = (token_t){
	    .type = type,
	    .location = location,
	    .lexeme = NULL,
	    .length = 0,
	};

	return LEXER_RESULT_OK;
}

/**
 * @brief Grows the indentation-width stack when a line opens a new block.
 *
 * @param lexer Lexer whose indentation stack to grow.
 * @return LEXER_RESULT_OK on success, otherwise an allocation error.
 */
static lexer_result_t grow_indentation_stack(lexer_t *lexer) {
	// The stack is always allocated with at least one element, so the next push is safe.
	if (lexer->indent_count + 1 < lexer->indentation_capacity) {
		return LEXER_RESULT_OK;
	}

	// Prevent overflow when doubling the stack capacity.
	if (lexer->indentation_capacity > SIZE_MAX / (2 * sizeof(*lexer->indentation_widths))) {
		lexer_set_error(lexer, LEXER_RESULT_MEMORY_ERROR, "indentation nesting is too deep >///<");
		return lexer->error.code;
	}

	// Double it... the stack capacity and reallocate it.
	size_t capacity = lexer->indentation_capacity * 2;
	size_t *widths = realloc(lexer->indentation_widths, capacity * sizeof(*widths));
	if (widths == NULL) {
		lexer_set_error(lexer, LEXER_RESULT_MEMORY_ERROR, "cannot grow indentation state");
		return lexer->error.code;
	}

	// Update the lexer with the new stack and capacity.
	lexer->indentation_widths = widths;
	lexer->indentation_capacity = capacity;

	return LEXER_RESULT_OK;
}

/**
 * @brief Skips a // comment and leaves its LF unread.
 *
 * @param lexer Lexer reading the comment opening.
 * @return LEXER_RESULT_OK on success, otherwise an error code.
 */
static lexer_result_t skip_line_comment(lexer_t *lexer) {
	// Skip the // comment opening.
	lexer_result_t result = consume_input(lexer);
	if (result != LEXER_RESULT_OK) {
		return result;
	}

	result = consume_input(lexer);
	if (result != LEXER_RESULT_OK) {
		return result;
	}

	// Skip the comment text until EOF or LF, but do not consume the LF.
	while (lexer->input != EOF && lexer->input != '\n') {
		if (lexer->input > 127) {
			lexer_set_error(lexer, LEXER_RESULT_LEXICAL_ERROR, "source contains a non-ASCII character");
			return lexer->error.code;
		}

		result = consume_input(lexer);
		if (result != LEXER_RESULT_OK) {
			return result;
		}
	}

	return LEXER_RESULT_OK;
}

/**
 * @brief Skips one possibly nested block comment.
 *
 * @param lexer Lexer reading the comment opening.
 * @param contains_newline Receives true if the comment contains LF.
 * @return LEXER_RESULT_OK on success, otherwise an error code.
 */
static lexer_result_t skip_block_comment(lexer_t *lexer, bool *contains_newline) {
	*contains_newline = false;
	lexer->comment_depth = 1;

	// Skip the /* comment opening.
	lexer_result_t result = consume_input(lexer);
	if (result != LEXER_RESULT_OK) {
		return result;
	}

	result = consume_input(lexer);
	if (result != LEXER_RESULT_OK) {
		return result;
	}

	// Skip the comment text until EOF or the matching */.
	while (lexer->comment_depth != 0) {
		if (lexer->input == EOF) {
			lexer_set_error(lexer, LEXER_RESULT_LEXICAL_ERROR, "unterminated block comment");
			return lexer->error.code;
		}

		if (lexer->input == '\n') {
			*contains_newline = true;
		}

		if (lexer->input > 127) {
			lexer_set_error(lexer, LEXER_RESULT_LEXICAL_ERROR, "source contains a non-ASCII character");
			return lexer->error.code;
		}

		// Handle nested block comments, which are allowed in IFJ26.
		if (lexer->input == '/' && peek_input(lexer) == '*') {
			result = consume_input(lexer);
			if (result == LEXER_RESULT_OK) {
				result = consume_input(lexer);
			}

			if (result != LEXER_RESULT_OK) {
				return result;
			}

			++lexer->comment_depth;
			continue;
		}

		// Handle block comment closure, which may be nested.
		if (lexer->input == '*' && peek_input(lexer) == '/') {
			result = consume_input(lexer);
			if (result == LEXER_RESULT_OK) {
				result = consume_input(lexer);
			}

			if (result != LEXER_RESULT_OK) {
				return result;
			}

			--lexer->comment_depth;
			continue;
		}

		result = consume_input(lexer);
		if (result != LEXER_RESULT_OK) {
			return result;
		}
	}

	return LEXER_RESULT_OK;
}

/**
 * @brief Validates and consumes the remainder of a multiline-comment line.
 *
 * @param lexer Lexer reading the comment.
 * @return LEXER_RESULT_OK on success, otherwise a lexical error.
 */
static lexer_result_t finish_multiline_comment_line(lexer_t *lexer) {
	// Skip any whitespace after the comment
	while (lexer->input == ' ' || lexer->input == '\t' || lexer->input == '\r') {
		lexer_result_t result = consume_input(lexer);
		if (result != LEXER_RESULT_OK) {
			return result;
		}
	}

	// If the next character is not LF or EOF, then the multiline comment was not followed by a newline.
	if (lexer->input != EOF && lexer->input != '\n') {
		lexer_set_error(lexer, LEXER_RESULT_LEXICAL_ERROR, "token after a multiline block comment");
		return lexer->error.code;
	}

	// If the next character is LF, then consume it so that the next token starts on the following line.
	if (lexer->input == '\n') {
		return consume_input(lexer);
	}

	return LEXER_RESULT_OK;
}

/**
 * @brief Emits indentation tokens when a meaningful line changes block depth.
 *
 * The stack stores only prior indentation widths. A dedent can correspond to
 * several closing blocks, so this function emits the first DEDENT and stores
 * the remaining count in pending_dedents for later scanner calls.
 *
 * @param lexer Lexer whose indentation stack to update.
 * @param width Count of leading spaces on the meaningful line.
 * @param out_token Receives INDENT or DEDENT when required.
 * @return LEXER_RESULT_OK on success, otherwise a lexical error.
 */
static lexer_result_t emit_indentation(lexer_t *lexer, size_t width, token_t *out_token) {
	size_t current_width = lexer->indentation_widths[lexer->indent_count];
	token_location_t location = {lexer->line, lexer->column};

	// If the current line's indentation matches the previous line's, nothing is emitted.
	if (width == current_width) {
		return LEXER_RESULT_OK;
	}

	// If the current line's indentation is greater than the previous line's, push the new width onto the stack.
	if (width > current_width) {
		lexer_result_t result = grow_indentation_stack(lexer);
		if (result != LEXER_RESULT_OK) {
			return result;
		}

		// Push the new width onto the stack and emit an INDENT token.
		lexer->indentation_widths[++lexer->indent_count] = width;
		return make_layout_token(out_token, TOKEN_INDENT, location);
	}

	size_t matching_level = lexer->indent_count;

	// If the indentation is less than the previous line's, pop widths from the stack.
	while (matching_level > 0 && lexer->indentation_widths[matching_level] != width) {
		--matching_level;
	}

	// If no matching width is found, report an indentation error.
	if (lexer->indentation_widths[matching_level] != width) {
		lexer_set_error(lexer, LEXER_RESULT_LEXICAL_ERROR, "inconsistent indentation width");
		return lexer->error.code;
	}

	// If a matching width is found, emit a DEDENT token and update the stack.
	lexer->pending_dedents = lexer->indent_count - matching_level - 1;
	lexer->indent_count = matching_level;

	return make_layout_token(out_token, TOKEN_DEDENT, location);
}

/**
 * @brief Skips ignored physical lines and reads next meaningful-line indentation.
 *
 * @param lexer Lexer to process.
 * @param out_token Receives a generated indentation token.
 * @param emitted Receives true if out_token was generated.
 * @return LEXER_RESULT_OK on success, otherwise an error code.
 */
static lexer_result_t prepare_line_start(lexer_t *lexer, token_t *out_token, bool *emitted) {
	size_t width = 0;
	*emitted = false;

	/*
     * Process physical lines until a line containing meaningful input is found.
     * Empty lines and comment-only lines do not affect indentation.
     */
	for (;;) {

		// Count leading spaces to determine the indentation width
		while (lexer->input == ' ') {
			++width;

			lexer_result_t result = consume_input(lexer);
			if (result != LEXER_RESULT_OK) {
				return result;
			}
		}

		// Handle CRLF line endings as a single newline.
		// clrf should end with lexical error
		if (lexer->input == '\r' && peek_input(lexer) == '\n') {
			lexer_result_t result = consume_input(lexer);
			if (result != LEXER_RESULT_OK) {
				return result;
			}

			continue;
		}

		// Tabs are not allowed for indentation in IFJ26.
		if (lexer->input == '\t') {
			lexer_set_error(lexer, LEXER_RESULT_LEXICAL_ERROR, "tab used for indentation");
			return lexer->error.code;
		}

		// If the current line is empty, then no indentation token is emitted.
		if (lexer->input == '\n') {
			lexer_result_t result = consume_input(lexer);
			if (result != LEXER_RESULT_OK) {
				return result;
			}

			width = 0;
			continue;
		}

		// Skip line comments
		if (lexer->input == '/' && peek_input(lexer) == '/') {
			lexer_result_t result = skip_line_comment(lexer);
			if (result != LEXER_RESULT_OK) {
				return result;
			}

			if (lexer->input == '\n') {
				result = consume_input(lexer);
				if (result != LEXER_RESULT_OK) {
					return result;
				}
			}

			width = 0;
			continue;
		}

		// Skip block comments, which may be nested and may contain newlines.
		if (lexer->input == '/' && peek_input(lexer) == '*') {
			bool contains_newline = false;
			lexer_result_t result = skip_block_comment(lexer, &contains_newline);
			if (result != LEXER_RESULT_OK) {
				return result;
			}

			// If the block comment contains a newline, then finish the line and reset the indentation width.
			if (contains_newline) {
				result = finish_multiline_comment_line(lexer);
				if (result != LEXER_RESULT_OK) {
					return result;
				}

				width = 0;
			}

			continue;
		}

		// If the current line is empty, then no indentation token is emitted.
		if (lexer->input == EOF) {
			return LEXER_RESULT_OK;
		}

		lexer->at_line_start = false;
		lexer->logical_line_indent = width;

		lexer_result_t result = emit_indentation(lexer, width, out_token);
		if (result != LEXER_RESULT_OK) {
			return result;
		}

		if (out_token->type == TOKEN_INDENT || out_token->type == TOKEN_DEDENT) {
			*emitted = true;
		}

		return LEXER_RESULT_OK;
	}
}

/**
 * @brief Validates the indentation continuity  of a physical line in brackets.
 *
 * Newlines inside brackets are whitespace, and not TOKEN_NEWLINE.
 *
 * @param lexer Lexer to use.
 * @return LEXER_RESULT_OK on success, otherwise a lexical error.
 */
static lexer_result_t prepare_parenthesized_line_start(lexer_t *lexer) {
	size_t width = 0;

	// Skip leading whitespace and comments until a non-whitespace character is found.
	for (;;) {
		while (lexer->input == ' ') {
			++width;

			lexer_result_t result = consume_input(lexer);
			if (result != LEXER_RESULT_OK) {
				return result;
			}
		}

		if (lexer->input == '\r' && peek_input(lexer) == '\n') {
			lexer_result_t result = consume_input(lexer);
			if (result != LEXER_RESULT_OK) {
				return result;
			}

			continue;
		}

		if (lexer->input == '\t') {
			lexer_set_error(lexer, LEXER_RESULT_LEXICAL_ERROR, "tab used for continuation indentation");
			return lexer->error.code;
		}

		if (lexer->input == '\n') {
			lexer_result_t result = consume_input(lexer);
			if (result != LEXER_RESULT_OK) {
				return result;
			}

			width = 0;
			continue;
		}

		if (lexer->input == '/' && peek_input(lexer) == '/') {
			lexer_result_t result = skip_line_comment(lexer);
			if (result != LEXER_RESULT_OK) {
				return result;
			}

			if (lexer->input == '\n') {
				result = consume_input(lexer);
				if (result != LEXER_RESULT_OK) {
					return result;
				}
			}

			width = 0;
			continue;
		}

		if (lexer->input == '/' && peek_input(lexer) == '*') {
			bool contains_newline = false;
			lexer_result_t result = skip_block_comment(lexer, &contains_newline);
			if (result != LEXER_RESULT_OK) {
				return result;
			}

			if (contains_newline) {
				result = finish_multiline_comment_line(lexer);
				if (result != LEXER_RESULT_OK) {
					return result;
				}

				width = 0;
			}

			continue;
		}

		if (lexer->input == EOF) {
			return LEXER_RESULT_OK;
		}

		if (width <= lexer->logical_line_indent) {
			lexer_set_error(lexer, LEXER_RESULT_LEXICAL_ERROR,
			                "continuation line is not indented beyond its logical line");
			return lexer->error.code;
		}

		lexer->at_line_start = false;
		return LEXER_RESULT_OK;
	}
}

/**
 * @brief Scans an identifier and determines its type.
 *
 * @param lexer Lexer at an identifier start.
 * @param out_token Receives the recognized token.
 * @return LEXER_RESULT_OK on success, otherwise an error code.
 */
static lexer_result_t scan_identifier(lexer_t *lexer, token_t *out_token) {
	token_location_t location = {lexer->line, lexer->column};
	text_builder_t builder = {0};

	while (is_identifier_part(lexer->input)) {
		lexer_result_t result = take_input(lexer, &builder);
		if (result != LEXER_RESULT_OK) {
			builder_destroy(&builder);
			return result;
		}
	}

	// Determine the token type based on the identifier's text.
	token_type_t type = builder.length == 1 && builder.data[0] == '_'
	                        ? TOKEN_WILDCARD
	                        : lexer_keyword_type(builder.data, builder.length);

	return make_token(out_token, type, location, &builder);
}

/**
 * @brief Scans digits separated only by valid underscore separators.
 *
 * @param lexer Lexer to use.
 * @param builder Builder receiving source text.
 * @param has_digits Receives whether one or more digits were consumed.
 * @return LEXER_RESULT_OK on success, otherwise a lexical error.
 */
static lexer_result_t scan_digit_sequence(lexer_t *lexer, text_builder_t *builder, bool *has_digits) {
	bool previous_was_digit = false;
	*has_digits = false;

	// Consume digits and underscores, validating that underscores are not at the start, end, or adjacent to other underscores.
	while (is_digit(lexer->input) || lexer->input == '_') {
		if (lexer->input == '_') {
			if (!previous_was_digit || !is_digit(peek_input(lexer))) {
				lexer_set_error(lexer, LEXER_RESULT_LEXICAL_ERROR, "invalid underscore in number");
				return lexer->error.code;
			}

			previous_was_digit = false;
		} else {
			*has_digits = true;
			previous_was_digit = true;
		}

		lexer_result_t result = take_input(lexer, builder);
		if (result != LEXER_RESULT_OK) {
			return result;
		}
	}

	return LEXER_RESULT_OK;
}

/**
 * @brief Scans an integer or double literal.
 *
 * @param lexer Lexer to use.
 * @param out_token Receives the recognized token.
 * @return LEXER_RESULT_OK on success, otherwise an error code.
 */
static lexer_result_t scan_number(lexer_t *lexer, token_t *out_token) {
	token_location_t location = {lexer->line, lexer->column};
	text_builder_t builder = {0};
	bool has_digits = false;
	bool has_fraction = false;
	bool has_exponent = false;
	bool has_double_suffix = false;

	if (lexer->input == '.') {
		has_fraction = true;

		lexer_result_t result = take_input(lexer, &builder);
		if (result != LEXER_RESULT_OK) {
			builder_destroy(&builder);
			return result;
		}

		// A decimal fraction must have at least one digit after the decimal point.
		result = scan_digit_sequence(lexer, &builder, &has_digits);
		if (result != LEXER_RESULT_OK || !has_digits) {
			if (result == LEXER_RESULT_OK) {
				lexer_set_error(lexer, LEXER_RESULT_LEXICAL_ERROR, "missing decimal fraction");
			}

			builder_destroy(&builder);
			return lexer->error.code;
		}
	} else {
		int first_digit = lexer->input;

		lexer_result_t result = scan_digit_sequence(lexer, &builder, &has_digits);
		if (result != LEXER_RESULT_OK) {
			builder_destroy(&builder);
			return result;
		}

		// A nonzero number cannot start with a leading zero.
		if (first_digit == '0' && builder.length > 1) {
			lexer_set_error(lexer, LEXER_RESULT_LEXICAL_ERROR, "nonzero number starts with zero");
			builder_destroy(&builder);
			return lexer->error.code;
		}
	}

	// If a decimal point is present, then a decimal fraction must follow.
	if (lexer->input == '.') {
		has_fraction = true;

		lexer_result_t result = take_input(lexer, &builder);
		if (result != LEXER_RESULT_OK) {
			builder_destroy(&builder);
			return result;
		}

		// A decimal fraction must have at least one digit after the decimal point.
		bool fraction_digits = false;
		result = scan_digit_sequence(lexer, &builder, &fraction_digits);
		if (result != LEXER_RESULT_OK || !fraction_digits) {
			if (result == LEXER_RESULT_OK) {
				lexer_set_error(lexer, LEXER_RESULT_LEXICAL_ERROR, "missing decimal fraction");
			}

			builder_destroy(&builder);
			return lexer->error.code;
		}
	}

	// If an exponent is present, then it must be followed by an optional sign and at least one digit.
	if (lexer->input == 'e' || lexer->input == 'E') {
		has_exponent = true;

		lexer_result_t result = take_input(lexer, &builder);
		if (result != LEXER_RESULT_OK) {
			builder_destroy(&builder);
			return result;
		}

		if (lexer->input == '+' || lexer->input == '-') {
			result = take_input(lexer, &builder);
			if (result != LEXER_RESULT_OK) {
				builder_destroy(&builder);
				return result;
			}
		}

		bool exponent_digits = false;
		result = scan_digit_sequence(lexer, &builder, &exponent_digits);
		if (result != LEXER_RESULT_OK || !exponent_digits) {
			if (result == LEXER_RESULT_OK) {
				lexer_set_error(lexer, LEXER_RESULT_LEXICAL_ERROR, "missing exponent digits");
			}

			builder_destroy(&builder);
			return lexer->error.code;
		}
	}

	// If a double suffix is present, then it must be the last character of the number.
	if (lexer->input == 'd' || lexer->input == 'D') {
		has_double_suffix = true;

		lexer_result_t result = take_input(lexer, &builder);
		if (result != LEXER_RESULT_OK) {
			builder_destroy(&builder);
			return result;
		}
	}

	if (is_identifier_part(lexer->input)) {
		lexer_set_error(lexer, LEXER_RESULT_LEXICAL_ERROR, "identifier immediately follows number");
		builder_destroy(&builder);
		return lexer->error.code;
	}

	token_type_t type = has_fraction || has_exponent || has_double_suffix ? TOKEN_DOUBLE : TOKEN_INTEGER;
	return make_token(out_token, type, location, &builder);
}

/**
 * @brief Scans a normal or triple-quoted multiline string.
 *
 * @param lexer Lexer at an opening quote.
 * @param out_token Receives the recognized token.
 * @return LEXER_RESULT_OK on success, otherwise a lexical error.
 */
static lexer_result_t scan_string(lexer_t *lexer, token_t *out_token) {
	token_location_t location = {lexer->line, lexer->column};
	text_builder_t builder = {0};
	lexer_result_t result = take_input(lexer, &builder);
	if (result != LEXER_RESULT_OK) {
		builder_destroy(&builder);
		return result;
	}

	bool multiline = lexer->input == '"' && peek_input(lexer) == '"';

	if (multiline) {
		result = take_input(lexer, &builder);
		if (result == LEXER_RESULT_OK) {
			result = take_input(lexer, &builder);
		}

		if (result != LEXER_RESULT_OK) {
			builder_destroy(&builder);
			return result;
		}

		size_t quote_count = 0;

		// Consume characters until the closing triple quotes are found.
		while (lexer->input != EOF) {
			if (lexer->input > 127) {
				lexer_set_error(lexer, LEXER_RESULT_LEXICAL_ERROR, "source contains a non-ASCII character");
				builder_destroy(&builder);
				return lexer->error.code;
			}

			quote_count = lexer->input == '"' ? quote_count + 1 : 0;

			result = take_input(lexer, &builder);
			if (result != LEXER_RESULT_OK) {
				builder_destroy(&builder);
				return result;
			}

			if (quote_count == 3) {
				return make_token(out_token, TOKEN_MULTILINE_STRING, location, &builder);
			}
		}

		lexer_set_error(lexer, LEXER_RESULT_LEXICAL_ERROR, "unterminated multiline string");
		builder_destroy(&builder);
		return lexer->error.code;
	}

	while (lexer->input != EOF && lexer->input != '"' && lexer->input != '\n') {
		if (lexer->input == '\\') {
			result = take_input(lexer, &builder);
			if (result != LEXER_RESULT_OK) {
				builder_destroy(&builder);
				return result;
			}

			// Validate that the escape sequence is one of the allowed characters.
			if (lexer->input != '\\' && lexer->input != '"' && lexer->input != 'n' && lexer->input != 'r' &&
			    lexer->input != 't') {
				lexer_set_error(lexer, LEXER_RESULT_LEXICAL_ERROR, "invalid string escape");
				builder_destroy(&builder);
				return lexer->error.code;
			}
		}

		if (lexer->input > 127) {
			lexer_set_error(lexer, LEXER_RESULT_LEXICAL_ERROR, "source contains a non-ASCII character");
			builder_destroy(&builder);
			return lexer->error.code;
		}

		result = take_input(lexer, &builder);
		if (result != LEXER_RESULT_OK) {
			builder_destroy(&builder);
			return result;
		}
	}

	// If the string was not terminated by a closing quote, report an error.
	if (lexer->input != '"') {
		lexer_set_error(lexer, LEXER_RESULT_LEXICAL_ERROR, "unterminated string");
		builder_destroy(&builder);
		return lexer->error.code;
	}

	result = take_input(lexer, &builder);
	if (result != LEXER_RESULT_OK) {
		builder_destroy(&builder);
		return result;
	}

	return make_token(out_token, TOKEN_STRING, location, &builder);
}

/**
 * @brief Scans the @main declaration.
 *
 * @param lexer Lexer to use.
 * @param out_token Receives TOKEN_MAIN_ANNOTATION.
 * @return LEXER_RESULT_OK on success, otherwise a lexical error.
 */
static lexer_result_t scan_main_declaration(lexer_t *lexer, token_t *out_token) {
	static const char ANNOTATION[] = "@main";
	token_location_t location = {lexer->line, lexer->column};
	text_builder_t builder = {0};

	// Consume the characters of the annotation and validate that they match.
	for (size_t index = 0; ANNOTATION[index] != '\0'; ++index) {
		if (lexer->input != ANNOTATION[index]) {
			lexer_set_error(lexer, LEXER_RESULT_LEXICAL_ERROR, "unknown annotation");
			builder_destroy(&builder);
			return lexer->error.code;
		}

		lexer_result_t result = take_input(lexer, &builder);
		if (result != LEXER_RESULT_OK) {
			builder_destroy(&builder);
			return result;
		}
	}

	// Check that the annotation is not followed by an identifier character. E.g., "@mainx"
	if (is_identifier_part(lexer->input)) {
		lexer_set_error(lexer, LEXER_RESULT_LEXICAL_ERROR, "unknown annotation");
		builder_destroy(&builder);
		return lexer->error.code;
	}

	return make_token(out_token, TOKEN_MAIN_ANNOTATION, location, &builder);
}

/**
 * @brief Updates the nesting count used to track indentation.
 *
 * @param lexer Lexer to use
 * @param type Newly recognized delimiter.
 */
static void update_parenthesis_depth(lexer_t *lexer, token_type_t type) {
	if (type == TOKEN_LPAREN || type == TOKEN_LBRACKET) {
		++lexer->parenthesis_depth;
	} else if ((type == TOKEN_RPAREN || type == TOKEN_RBRACKET) && lexer->parenthesis_depth != 0) {
		--lexer->parenthesis_depth;
	}
}

/**
 * @brief Scans punctuation and operators with longest-match decisions.
 *
 * @param lexer Lexer to use.
 * @param out_token Receives the recognized token.
 * @return LEXER_RESULT_OK on success, otherwise a lexical error.
 */
static lexer_result_t scan_symbol(lexer_t *lexer, token_t *out_token) {
	token_location_t location = {lexer->line, lexer->column};
	text_builder_t builder = {0};
	token_type_t type;
	int first = lexer->input;

	// Consume the first character of the symbol and append it to the builder.
	lexer_result_t result = take_input(lexer, &builder);
	if (result != LEXER_RESULT_OK) {
		builder_destroy(&builder);
		return result;
	}

	switch (first) {
	case '(':
		type = TOKEN_LPAREN;
		break;
	case ')':
		type = TOKEN_RPAREN;
		break;
	case '[':
		type = TOKEN_LBRACKET;
		break;
	case ']':
		type = TOKEN_RBRACKET;
		break;
	case ',':
		type = TOKEN_COMMA;
		break;
	case ';':
		type = TOKEN_SEMICOLON;
		break;
	case '.':
		type = TOKEN_DOT;
		break;
	case ':':
		type = TOKEN_COLON;
		break;
	case '+':
		type = TOKEN_PLUS;
		if (lexer->input == '+') {
			result = take_input(lexer, &builder);
			type = TOKEN_CONCAT;
		}
		break;
	case '-':
		type = TOKEN_MINUS;
		break;
	case '*':
		type = TOKEN_MULTIPLY;
		break;
	case '/':
		type = TOKEN_DIVIDE;
		break;
	case '|':
		type = TOKEN_PIPE;
		if (lexer->input == '|') {
			result = take_input(lexer, &builder);
			type = TOKEN_OR;
		}
		break;
	case '&':
		if (lexer->input != '&') {
			lexer_set_error(lexer, LEXER_RESULT_LEXICAL_ERROR, "expected second ampersand");
			builder_destroy(&builder);
			return lexer->error.code;
		}
		result = take_input(lexer, &builder);
		type = TOKEN_AND;
		break;
	case '=':
		type = TOKEN_ASSIGN;
		if (lexer->input == '=') {
			result = take_input(lexer, &builder);
			type = TOKEN_EQUAL;
		} else if (lexer->input == '>') {
			result = take_input(lexer, &builder);
			type = TOKEN_FAT_ARROW;
		}
		break;
	case '!':
		type = TOKEN_NOT;
		if (lexer->input == '=') {
			result = take_input(lexer, &builder);
			type = TOKEN_NOT_EQUAL;
		}
		break;
	case '<':
		type = TOKEN_LESS;
		if (lexer->input == '=') {
			result = take_input(lexer, &builder);
			type = TOKEN_LESS_EQUAL;
		} else if (lexer->input == '-') {
			result = take_input(lexer, &builder);
			type = TOKEN_LEFT_ARROW;
		}
		break;
	case '>':
		type = TOKEN_GREATER;
		if (lexer->input == '=') {
			result = take_input(lexer, &builder);
			type = TOKEN_GREATER_EQUAL;
		}
		break;
	default:
		lexer_set_error(lexer, LEXER_RESULT_LEXICAL_ERROR, "unknown input character");
		builder_destroy(&builder);
		return lexer->error.code;
	}

	if (result != LEXER_RESULT_OK) {
		builder_destroy(&builder);
		return result;
	}

	update_parenthesis_depth(lexer, type);
	return make_token(out_token, type, location, &builder);
}

/**
 * @brief Scans and produces exactly one IFJ26 terminal token.
 *
 * @param lexer Lexer to use.
 * @param out_token Token receiving the next terminal.
 * @return LEXER_RESULT_OK on success, otherwise an error code.
 */
lexer_result_t lexer_scan_token(lexer_t *lexer, token_t *out_token) {
	if (lexer == NULL || out_token == NULL) {
		return LEXER_RESULT_IO_ERROR;
	}

	// Shall we clear thy output token before scanning?
	*out_token = (token_t){0};

	for (;;) {
		// If there are pending DEDENT tokens, emit them first.
		if (lexer->pending_dedents != 0) {
			--lexer->pending_dedents;
			return make_layout_token(out_token, TOKEN_DEDENT, (token_location_t){lexer->line, lexer->column});
		}

		if (lexer->input == EOF) {
			if (ferror(lexer->source)) {
				lexer_set_error(lexer, LEXER_RESULT_IO_ERROR, "cannot read source input");
				return lexer->error.code;
			}

			// if we are at EOF, we need to emit any remaining DEDENT tokens before emitting EOF
			if (lexer->indent_count != 0) {
				lexer->pending_dedents = lexer->indent_count - 1;
				lexer->indent_count = 0;
				return make_layout_token(out_token, TOKEN_DEDENT,
				                         (token_location_t){lexer->line, lexer->column});
			}

			return make_layout_token(out_token, TOKEN_EOF, (token_location_t){lexer->line, lexer->column});
		}

		// Validate that the input character is ASCII.
		if (lexer->input > 127) {
			lexer_set_error(lexer, LEXER_RESULT_LEXICAL_ERROR, "source contains a non-ASCII character");
			return lexer->error.code;
		}

		// Handle line starts
		if (lexer->at_line_start) {
			if (lexer->parenthesis_depth == 0) {
				bool emitted = false;
				lexer_result_t result = prepare_line_start(lexer, out_token, &emitted);
				if (result != LEXER_RESULT_OK) {
					return result;
				}

				if (emitted || lexer->input == EOF) {
					if (emitted) {
						return LEXER_RESULT_OK;
					}

					continue;
				}
			} else {
				// If we are inside parentheses, then newlines are whitespace and do not affect indentation.
				lexer_result_t result = prepare_parenthesized_line_start(lexer);
				if (result != LEXER_RESULT_OK) {
					return result;
				}

				if (lexer->input == EOF) {
					continue;
				}
			}
		}

		// Skip whitespace and comments until a non-whitespace character is found.
		if (lexer->input == ' ' || lexer->input == '\t') {
			lexer_result_t result = consume_input(lexer);
			if (result != LEXER_RESULT_OK) {
				return result;
			}

			continue;
		}

		// Handle CRLF line endings as a single newline.
		if (lexer->input == '\r') {
			if (peek_input(lexer) != '\n') {
				lexer_set_error(lexer, LEXER_RESULT_LEXICAL_ERROR, "bare carriage return in source");
				return lexer->error.code;
			}

			lexer_result_t result = consume_input(lexer);
			if (result != LEXER_RESULT_OK) {
				return result;
			}

			continue;
		}

		// If the current line is empty, then no indentation token is emitted.
		if (lexer->input == '\n') {
			token_location_t location = {lexer->line, lexer->column};

			lexer_result_t result = consume_input(lexer);
			if (result != LEXER_RESULT_OK) {
				return result;
			}

			lexer->at_line_start = true;

			if (lexer->parenthesis_depth != 0) {
				continue;
			}

			return make_layout_token(out_token, TOKEN_NEWLINE, location);
		}

		// Skip line comments
		if (lexer->input == '/' && peek_input(lexer) == '/') {
			lexer_result_t result = skip_line_comment(lexer);
			if (result != LEXER_RESULT_OK) {
				return result;
			}

			continue;
		}

		// Skip block comments, which may be nested and may contain newlines.
		if (lexer->input == '/' && peek_input(lexer) == '*') {
			bool contains_newline = false;
			lexer_result_t result = skip_block_comment(lexer, &contains_newline);
			if (result != LEXER_RESULT_OK) {
				return result;
			}

			if (!contains_newline) {
				continue;
			}

			result = finish_multiline_comment_line(lexer);
			if (result != LEXER_RESULT_OK) {
				return result;
			}

			lexer->at_line_start = true;

			if (lexer->parenthesis_depth != 0) {
				continue;
			}

			return make_layout_token(out_token, TOKEN_NEWLINE,
			                         (token_location_t){lexer->line, lexer->column});
		}

		lexer_result_t result;

		// Determine the type of token
		if (is_identifier_start(lexer->input)) {
			result = scan_identifier(lexer, out_token);
		} else if (is_digit(lexer->input) || (lexer->input == '.' && is_digit(peek_input(lexer)))) {
			result = scan_number(lexer, out_token);
		} else if (lexer->input == '"') {
			result = scan_string(lexer, out_token);
		} else if (lexer->input == '@') {
			result = scan_main_declaration(lexer, out_token);
		} else {
			result = scan_symbol(lexer, out_token);
		}

		if (result != LEXER_RESULT_OK) {
			return result;
		}

		lexer->at_line_start = false;
		return LEXER_RESULT_OK;
	}
}
