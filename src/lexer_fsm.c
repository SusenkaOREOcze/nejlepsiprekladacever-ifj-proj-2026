/**
 * @file lexer_fsm.c
 * @author xkralil00 (SusenkaOREOcze)
 * @date 2026-10-10
 * @brief Alternative finite-state scanner for IFJ26 terminals.
 *
 * @details The diagram shows the state machine for the lexer.
 *
 * Scanner diagram: \dotfile lexer_automaton.dot "Lexer automaton"
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
 * @param lexer Lexer requesting more space.
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
 * @param lexer Lexer appending to the builder.
 * @param builder Builder to append to.
 * @param character Character to append.
 * @return LEXER_RESULT_OK on success, otherwise an error code.
 */
static lexer_result_t builder_append(lexer_t *lexer, text_builder_t *builder, int character) {
	if (builder->length > SIZE_MAX - 2) {
		lexer_set_error(lexer, LEXER_RESULT_MEMORY_ERROR, "token text is too long");
		return LEXER_RESULT_MEMORY_ERROR;
	}

	// Eating too much cake is the sin of gluttony.
	// However, eating too much pie is ok because
	//
	// the Sin of Pi is 0.
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
 * @param builder The noble builder, relinquishing dominion over its text.
 * @return Pointer to the text which hath been liberated from its former master.
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
 * @param lexer Lexer whose input to advance.
 * @return LEXER_RESULT_OK on success, otherwise an I/O error.
 */
static lexer_result_t advance_input(lexer_t *lexer) {
	if (lexer->input == EOF) {
		return LEXER_RESULT_OK;
	}

	// Check for overflow of the source position counters before consuming the input character.
	// The file has to many lines or columns, we cannot count them anymore.
	int consumed = lexer->input;
	if ((consumed == '\n' && lexer->line == SIZE_MAX) || (consumed != '\n' && lexer->column == SIZE_MAX)) {
		lexer_set_error(lexer, LEXER_RESULT_MEMORY_ERROR, "source position is too large");
		return lexer->error.code;
	}

	lexer->input = fgetc(lexer->source);

	// There seems to be a problem.
	if (lexer->input == EOF && ferror(lexer->source)) {
		lexer_set_error(lexer, LEXER_RESULT_IO_ERROR, "cannot read source input");
		return lexer->error.code;
	}

	// For those wondering why ++line and not line++;
	// It doesnt matter.
	if (consumed == '\n') {
		++lexer->line;
		lexer->column = 1;
	} else {
		++lexer->column;
	}

	return LEXER_RESULT_OK;
}

/**
 * @brief Peer at the following source character.
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
	// Imagine reading a book, and it suddenly ends in the middle of a word. Good cliffhanger.

	lexer_result_t result = builder_append(lexer, builder, lexer->input);
	if (result != LEXER_RESULT_OK) {
		return result;
	}

	return advance_input(lexer);
}

/**
 * @brief Consumes the current input character without retaining it.
 * It just means, SKIP.
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
 */
static void make_token(token_t *out_token, token_type_t type, token_location_t location,
                       text_builder_t *builder) {
	size_t length = builder->length;

	// Idk but, the recognized terminal type sounds like some kind of cancer being discovered.

	*out_token = (token_t){
	    .type = type,
	    .location = location,
	    .lexeme = builder_finish(builder),
	    .length = length,
	};
}

/**
 * @brief Creates a generated layout token with no source lexeme.
 *
 * @param out_token Token to fill.
 * @param type Generated layout terminal type.
 * @param location Position where the layout terminal occurs.
 */
static void make_layout_token(token_t *out_token, token_type_t type, token_location_t location) {
	*out_token = (token_t){
	    .type = type,
	    .location = location,
	    .lexeme = NULL,
	    .length = 0,
	};
}

/**
 * @brief Grows the indentation-width stack when a line opens a new block.
 *
 * @param lexer Lexer whose indentation stack to grow.
 * @return LEXER_RESULT_OK on success, otherwise an allocation error.
 */
static lexer_result_t grow_indentation_stack(lexer_t *lexer) {
	// For those that do not know (me), we check the stack capacity before growing.
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
		// dont i have a function for this? oh.. nope.. but i could... nah
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
 * @param first_newline Receives the first LF location, or NULL if unused.
 * @return LEXER_RESULT_OK on success, otherwise an error code.
 */
static lexer_result_t skip_block_comment(lexer_t *lexer, bool *contains_newline,
                                         token_location_t *first_newline) {
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
			if (!*contains_newline && first_newline != NULL) {
				*first_newline = (token_location_t){lexer->line, lexer->column};
			}
			*contains_newline = true;
		}

		// oh its here again...
		if (lexer->input > 127) {
			lexer_set_error(lexer, LEXER_RESULT_LEXICAL_ERROR, "source contains a non-ASCII character");
			return lexer->error.code;
		}

		// Handle nested block comments, which are allowed.
		if (lexer->input == '/' && peek_input(lexer) == '*') {
			if (lexer->comment_depth == SIZE_MAX) {
				lexer_set_error(lexer, LEXER_RESULT_MEMORY_ERROR, "block comment nesting is too deep >///<");
				return lexer->error.code;
			}

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

		// Consume any other character inside the comment.
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
	while (lexer->input == ' ' || lexer->input == '\t') {
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
	// In short, if the indentation stack is invalid, we cannot continue lexing.
	if (lexer->indentation_widths == NULL || lexer->indentation_capacity == 0 ||
	    lexer->indent_count >= lexer->indentation_capacity) {
		lexer_set_error(lexer, LEXER_RESULT_INVALID_ARGUMENT, "invalid indentation state");
		return lexer->error.code;
	}

	size_t current_width = lexer->indentation_widths[lexer->indent_count];
	token_location_t location = {lexer->line, lexer->column};

	// If the current line's indentation matches the previous line's, nothing happens.
	if (width == current_width) {
		return LEXER_RESULT_OK;
	}

	// If the current line's indentation is greater than the previous line's, push the new width onto the
	// stack.
	if (width > current_width) {
		lexer_result_t result = grow_indentation_stack(lexer);
		if (result != LEXER_RESULT_OK) {
			return result;
		}

		// Push the new width onto the stack and emit an INDENT token.
		lexer->indentation_widths[++lexer->indent_count] = width;
		make_layout_token(out_token, TOKEN_INDENT, location);
		return LEXER_RESULT_OK;
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

	make_layout_token(out_token, TOKEN_DEDENT, location);
	return LEXER_RESULT_OK;
}

/**
 * @brief Finds the first token-bearing physical line and counts its leading spaces.
 *
 * Blank and comment-only lines do not affect the width. The caller decides
 * whether the width opens a block or continues a line inside brackets.
 *
 * @param lexer Lexer to use.
 * @param width Leading spaces on the next meaningful line, or zero at EOF.
 * @return LEXER_RESULT_OK on success, otherwise an error code.
 */
static lexer_result_t read_line_prefix(lexer_t *lexer, size_t *width) {
	*width = 0;
	bool has_tab = false;
	bool count_indentation = true;

	for (;;) {
		while (lexer->input == ' ' || lexer->input == '\t') {
			if (count_indentation) {
				if (lexer->input == '\t') {
					has_tab = true;
				} else {
					if (*width == SIZE_MAX) {
						lexer_set_error(lexer, LEXER_RESULT_MEMORY_ERROR, "indentation width is too large");
						return lexer->error.code;
					}
					++*width;
				}
			}

			lexer_result_t result = consume_input(lexer);
			if (result != LEXER_RESULT_OK) {
				return result;
			}
		}

		if (lexer->input == '\r') {
			lexer_set_error(lexer, LEXER_RESULT_LEXICAL_ERROR, "carriage return is not a valid line ending");
			return lexer->error.code;
		}

		if (lexer->input == '\n') {
			lexer_result_t result = consume_input(lexer);
			if (result != LEXER_RESULT_OK) {
				return result;
			}
			*width = 0;
			has_tab = false;
			count_indentation = true;
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
			*width = 0;
			has_tab = false;
			count_indentation = true;
			continue;
		}

		if (lexer->input == '/' && peek_input(lexer) == '*') {
			bool contains_newline = false;
			lexer_result_t result = skip_block_comment(lexer, &contains_newline, NULL);
			if (result != LEXER_RESULT_OK) {
				return result;
			}

			if (contains_newline) {
				result = finish_multiline_comment_line(lexer);
				if (result != LEXER_RESULT_OK) {
					return result;
				}
				*width = 0;
				has_tab = false;
				count_indentation = true;
			} else {
				// Whitespace after a same-line comment is not leading indentation.
				count_indentation = false;
			}
			continue;
		}

		// An unfinished blank line at EOF has no indentation to validate.
		// Example: "     EOF" is a blank line with 5 spaces.
		if (lexer->input == EOF) {
			*width = 0;
			return LEXER_RESULT_OK;
		}

		if (has_tab) {
			lexer_set_error(lexer, LEXER_RESULT_LEXICAL_ERROR, "tab used for indentation");
			return lexer->error.code;
		}

		return LEXER_RESULT_OK;
	}
}

/*
    What is a logical line? A logical line is a sequence of physical lines that are connected
    together to form a single logical unit of code.
*/

/**
 * @brief Emits an indentation token when a meaningful logical line changes depth.
 *
 * @param lexer Lexer to use.
 * @param out_token Receives a generated indentation token, if any.
 * @param emitted Receives true when a token was generated.
 * @return LEXER_RESULT_OK on success, otherwise an error code.
 */
static lexer_result_t prepare_line_start(lexer_t *lexer, token_t *out_token, bool *emitted) {
	size_t width;
	*emitted = false;

	// Read the next meaningful line and count its leading spaces.
	lexer_result_t result = read_line_prefix(lexer, &width);
	if (result != LEXER_RESULT_OK || lexer->input == EOF) {
		return result;
	}

	lexer->at_line_start = false;
	lexer->logical_line_indent = width;

	// This is where the indentation token is emitted, if any.
	result = emit_indentation(lexer, width, out_token);
	if (result == LEXER_RESULT_OK) {
		*emitted = out_token->type == TOKEN_INDENT || out_token->type == TOKEN_DEDENT;
	}
	return result;
}

/*
    What is a continuation line? A continuation line is a physical line that continues the logical line
    that precedes it. Continuation lines are used to split long logical lines into multiple physical lines

    Example:
        # This is a logical line that continues onto the next physical line
        print("This is a long logical line that continues onto the next physical line"
              " and even onto the next one")
*/

/**
 * @brief Checks that a continuation line is indented past its logical line.
 *
 * @param lexer Lexer to process.
 * @return LEXER_RESULT_OK on success, otherwise an error code.
 */
static lexer_result_t prepare_parenthesized_line_start(lexer_t *lexer) {
	size_t width;
	lexer_result_t result = read_line_prefix(lexer, &width);
	if (result != LEXER_RESULT_OK || lexer->input == EOF) {
		return result;
	}

	// A continuation line must be indented beyond its logical line.
	if (width <= lexer->logical_line_indent) {
		lexer_set_error(lexer, LEXER_RESULT_LEXICAL_ERROR,
		                "continuation line is not indented beyond its logical line");
		return lexer->error.code;
	}

	lexer->at_line_start = false;
	return LEXER_RESULT_OK;
}

/**
 * @brief Scans an identifier and determines its type.
 *
 * @param lexer Lexer that READS.
 * @param out_token Receives the recognized token.
 * @return LEXER_RESULT_OK on success, otherwise an error code.
 */
static lexer_result_t scan_identifier(lexer_t *lexer, token_t *out_token) {
	token_location_t location = {lexer->line, lexer->column};
	text_builder_t builder = {0};

	// The first character is already known to be a valid identifier start.
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

	make_token(out_token, type, location, &builder);
	return LEXER_RESULT_OK;
}

/**
 * @brief Scans digits with optional number of underscores between digits.
 *
 * @param lexer Lexer to use.
 * @param builder Builder receiving source text.
 * @param has_digits Receives whether one or more digits were consumed.
 * @return LEXER_RESULT_OK on success, otherwise a lexical error.
 */
static lexer_result_t scan_digit_sequence(lexer_t *lexer, text_builder_t *builder, bool *has_digits) {
	bool ends_with_underscore = false;
	*has_digits = false;

	// Underscores are valid only after a digit and before another digit.
	while (is_digit(lexer->input) || lexer->input == '_') {
		if (lexer->input == '_') {
			if (!*has_digits) {
				lexer_set_error(lexer, LEXER_RESULT_LEXICAL_ERROR, "invalid underscore in number");
				return lexer->error.code;
			}

			ends_with_underscore = true;
		} else {
			*has_digits = true;
			ends_with_underscore = false;
		}

		lexer_result_t result = take_input(lexer, builder);
		if (result != LEXER_RESULT_OK) {
			return result;
		}
	}

	if (ends_with_underscore) {
		lexer_set_error(lexer, LEXER_RESULT_LEXICAL_ERROR, "invalid underscore in number");
		return lexer->error.code;
	}

	return LEXER_RESULT_OK;
}

/**
 * @brief Scans a mandatory digit sequence in a number component.
 *
 * @param lexer Lexer to use.
 * @param builder Builder receiving the source text.
 * @param missing_message Diagnostic if no digit follows.
 * @return LEXER_RESULT_OK on success, otherwise an error code.
 */
static lexer_result_t scan_required_digits(lexer_t *lexer, text_builder_t *builder,
                                           const char *missing_message) {
	bool has_digits = false;
	lexer_result_t result = scan_digit_sequence(lexer, builder, &has_digits);
	if (result != LEXER_RESULT_OK) {
		return result;
	}

	if (!has_digits) {
		lexer_set_error(lexer, LEXER_RESULT_LEXICAL_ERROR, missing_message);
		return lexer->error.code;
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
	lexer_result_t result;
	bool has_fraction = false;
	bool has_exponent = false;
	bool has_double_suffix = false;

	// The integer part is optional only when the literal starts with a dot.
	if (lexer->input != '.') {
		int first_digit = lexer->input;
		result = scan_required_digits(lexer, &builder, "missing integer digits");
		if (result != LEXER_RESULT_OK) {
			goto cleanup;
		}

		// Keep the original leading-zero rule for the integer part.
		if (first_digit == '0' && builder.length > 1) {
			lexer_set_error(lexer, LEXER_RESULT_LEXICAL_ERROR, "nonzero number starts with zero");
			result = lexer->error.code;
			goto cleanup;
		}
	}

	// A dot always starts a nonempty fractional part.
	if (lexer->input == '.') {
		has_fraction = true;
		result = take_input(lexer, &builder);
		if (result != LEXER_RESULT_OK) {
			goto cleanup;
		}

		result = scan_required_digits(lexer, &builder, "missing decimal fraction");
		if (result != LEXER_RESULT_OK) {
			goto cleanup;
		}
	}

	// Exponents have an optional sign followed by at least one digit.
	if (lexer->input == 'e' || lexer->input == 'E') {
		has_exponent = true;
		result = take_input(lexer, &builder);
		if (result != LEXER_RESULT_OK) {
			goto cleanup;
		}

		if (lexer->input == '+' || lexer->input == '-') {
			result = take_input(lexer, &builder);
			if (result != LEXER_RESULT_OK) {
				goto cleanup;
			}
		}

		result = scan_required_digits(lexer, &builder, "missing exponent digits");
		if (result != LEXER_RESULT_OK) {
			goto cleanup;
		}
	}

	// A d/D suffix also turns an integer-shaped spelling into a Double.
	if (lexer->input == 'd' || lexer->input == 'D') {
		has_double_suffix = true;
		result = take_input(lexer, &builder);
		if (result != LEXER_RESULT_OK) {
			goto cleanup;
		}
	}

	if (is_identifier_part(lexer->input)) {
		lexer_set_error(lexer, LEXER_RESULT_LEXICAL_ERROR, "identifier immediately follows number");
		result = lexer->error.code;
		goto cleanup;
	}

	token_type_t type = has_fraction || has_exponent || has_double_suffix ? TOKEN_DOUBLE : TOKEN_INTEGER;
	make_token(out_token, type, location, &builder);
	return LEXER_RESULT_OK;

cleanup:
	builder_destroy(&builder);
	return result;
}

/**
 * @brief Reads an ordinary string after its opening quote.
 *
 * @param lexer Lexer to use.
 * @param builder Receives the exact string.
 * @return LEXER_RESULT_OK after the closing quote, otherwise an error code.
 */
static lexer_result_t scan_ordinary_string_body(lexer_t *lexer, text_builder_t *builder) {
	// The opening quote was already consumed, so the body starts with the next character.
	while (lexer->input != EOF && lexer->input != '"' && lexer->input != '\n' && lexer->input != '\r') {
		// The five IFJ26 escape sequences are: \\, \", \n, \r, and \t.
		if (lexer->input == '\\') {
			lexer_result_t result = take_input(lexer, builder);
			if (result != LEXER_RESULT_OK) {
				return result;
			}

			// Only the five IFJ26 escape sequences are accepted.
			if (lexer->input != '\\' && lexer->input != '"' && lexer->input != 'n' && lexer->input != 'r' &&
			    lexer->input != 't') {
				lexer_set_error(lexer, LEXER_RESULT_LEXICAL_ERROR, "invalid string escape");
				return lexer->error.code;
			}
		}

		// we saw this somewhere already...
		if (lexer->input > 127) {
			lexer_set_error(lexer, LEXER_RESULT_LEXICAL_ERROR, "source contains a non-ASCII character");
			return lexer->error.code;
		}

		lexer_result_t result = take_input(lexer, builder);
		if (result != LEXER_RESULT_OK) {
			return result;
		}
	}

	// The string must be terminated by a closing quote.
	if (lexer->input != '"') {
		lexer_set_error(lexer, LEXER_RESULT_LEXICAL_ERROR, "unterminated string");
		return lexer->error.code;
	}

	return take_input(lexer, builder);
}

/**
 * @brief Reads a raw multiline string after its opening triple quote.
 *
 * @param lexer Lexer to use.
 * @param builder Receives the exact string.
 * @return LEXER_RESULT_OK after three closing quotes, otherwise an error code.
 */
static lexer_result_t scan_multiline_string_body(lexer_t *lexer, text_builder_t *builder) {
	size_t quote_count = 0;

	// The opening """ was already consumed, so the body starts with the next character.
	while (lexer->input != EOF) {
		// Yet another non-ASCII character check.
		if (lexer->input > 127) {
			lexer_set_error(lexer, LEXER_RESULT_LEXICAL_ERROR, "source contains a non-ASCII character");
			return lexer->error.code;
		}
		// i should move it to its own function :/

		// Count consecutive quotes to detect the closing triple quote.
		// doesnt this count """abc"de"fg" ??
		// no, look there is the : 0, which resets the counter.
		quote_count = lexer->input == '"' ? quote_count + 1 : 0;
		lexer_result_t result = take_input(lexer, builder);
		if (result != LEXER_RESULT_OK) {
			return result;
		}

		if (quote_count == 3) {
			return LEXER_RESULT_OK;
		}
	}

	lexer_set_error(lexer, LEXER_RESULT_LEXICAL_ERROR, "unterminated multiline string");
	return lexer->error.code;
}

/**
 * @brief Scans a normal or triple-quoted multiline string.
 *
 * @param lexer Lexer to enslave.
 * @param out_token Receives the recognized token.
 * @return LEXER_RESULT_OK on success, otherwise an error code.
 */
static lexer_result_t scan_string(lexer_t *lexer, token_t *out_token) {
	token_location_t location = {lexer->line, lexer->column};
	text_builder_t builder = {0};
	lexer_result_t result = take_input(lexer, &builder);
	if (result != LEXER_RESULT_OK) {
		goto cleanup;
	}

	// Check if the string is a multiline string by looking for a second quote.
	bool multiline = lexer->input == '"' && peek_input(lexer) == '"';
	if (multiline) {
		// The first opening quote was already consumed.
		result = take_input(lexer, &builder);
		if (result != LEXER_RESULT_OK) {
			goto cleanup;
		}
		result = take_input(lexer, &builder);
		if (result != LEXER_RESULT_OK) {
			goto cleanup;
		}

		// We have consumed all three opening quotes, so we can now scan the multiline string body.
		result = scan_multiline_string_body(lexer, &builder);
	} else {
		result = scan_ordinary_string_body(lexer, &builder);
	}

	if (result != LEXER_RESULT_OK) {
		goto cleanup;
	}

	make_token(out_token, multiline ? TOKEN_MULTILINE_STRING : TOKEN_STRING, location, &builder);
	return LEXER_RESULT_OK;

cleanup:
	builder_destroy(&builder);
	return result;
}

/**
 * @brief Scans the @main declaration.
 *
 * @note It was called anotation at the beginning, thats why it is anotation everywhere.
 *
 * @param lexer Lexer to use.
 * @param out_token Receives TOKEN_MAIN_ANNOTATION.
 * @return LEXER_RESULT_OK on success, otherwise a lexical error.
 */
static lexer_result_t scan_main_declaration(lexer_t *lexer, token_t *out_token) {
	static const char ANNOTATION[] = "@main";
	token_location_t location = {lexer->line, lexer->column};
	text_builder_t builder = {0};
	lexer_result_t result;

	// Consume the characters of the annotation and validate that they match.
	for (size_t index = 0; ANNOTATION[index] != '\0'; ++index) {
		if (lexer->input != ANNOTATION[index]) {
			lexer_set_error(lexer, LEXER_RESULT_LEXICAL_ERROR, "unknown annotation");
			result = lexer->error.code;
			goto cleanup;
		}

		result = take_input(lexer, &builder);
		if (result != LEXER_RESULT_OK) {
			goto cleanup;
		}
	}

	// Check that the annotation is not followed by an identifier character. E.g., "@mainx"
	if (is_identifier_part(lexer->input)) {
		lexer_set_error(lexer, LEXER_RESULT_LEXICAL_ERROR, "unknown annotation");
		result = lexer->error.code;
		goto cleanup;
	}

	make_token(out_token, TOKEN_MAIN_ANNOTATION, location, &builder);
	return LEXER_RESULT_OK;

cleanup:
	builder_destroy(&builder);
	return result;
}

/**
 * @brief Updates the nesting count used to track indentation.
 *
 * @param lexer Lexer to use
 * @param type Newly recognized delimiter.
 * @return LEXER_RESULT_OK on success, otherwise an error code.
 */
static lexer_result_t update_parenthesis_depth(lexer_t *lexer, token_type_t type) {
	if (type == TOKEN_LPAREN || type == TOKEN_LBRACKET) {
		if (lexer->parenthesis_depth == SIZE_MAX) {
			lexer_set_error(lexer, LEXER_RESULT_MEMORY_ERROR, "parenthesis nesting is too deep >///<");
			return lexer->error.code;
		}
		// we are checking opening brackets, so we increment the depth.
		++lexer->parenthesis_depth;

	} else if ((type == TOKEN_RPAREN || type == TOKEN_RBRACKET) && lexer->parenthesis_depth != 0) {
		// The opposite from the previous case.
		--lexer->parenthesis_depth;
	}

	return LEXER_RESULT_OK;
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
		goto cleanup;
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
			result = lexer->error.code;
			goto cleanup;
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
		result = lexer->error.code;
		goto cleanup;
	}

	if (result != LEXER_RESULT_OK) {
		goto cleanup;
	}

	result = update_parenthesis_depth(lexer, type);
	if (result != LEXER_RESULT_OK) {
		goto cleanup;
	}

	make_token(out_token, type, location, &builder);
	return LEXER_RESULT_OK;

cleanup:
	builder_destroy(&builder);
	return result;
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
		return LEXER_RESULT_INVALID_ARGUMENT;
	}

	// I am not explaining this...
	if (lexer->source == NULL || lexer->indentation_widths == NULL || lexer->indentation_capacity == 0 ||
	    lexer->indent_count >= lexer->indentation_capacity || lexer->indentation_widths[0] != 0 ||
	    lexer->pending_dedents >= lexer->indentation_capacity) { // here's the end of the IF
		lexer_set_error(lexer, LEXER_RESULT_INVALID_ARGUMENT, "invalid lexer state");
		return lexer->error.code;
	}

	// Shall we clear thy output token before scanning?
	*out_token = (token_t){0};

	for (;;) {
		// If there are pending DEDENT tokens, emit them first.
		if (lexer->pending_dedents != 0) {
			--lexer->pending_dedents;
			make_layout_token(out_token, TOKEN_DEDENT, (token_location_t){lexer->line, lexer->column});
			return LEXER_RESULT_OK;
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
				make_layout_token(out_token, TOKEN_DEDENT, (token_location_t){lexer->line, lexer->column});
				return LEXER_RESULT_OK;
			}

			make_layout_token(out_token, TOKEN_EOF, (token_location_t){lexer->line, lexer->column});
			return LEXER_RESULT_OK;
		}

		// Validate that the input character is ASCII.
		// why not use is_ascii_letter() ?
		// ehm, that checks only letters :3 te-heew
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

		// Skip whitespace
		if (lexer->input == ' ' || lexer->input == '\t') {
			lexer_result_t result = consume_input(lexer);
			if (result != LEXER_RESULT_OK) {
				return result;
			}

			continue;
		}

		if (lexer->input == '\r') {
			lexer_set_error(lexer, LEXER_RESULT_LEXICAL_ERROR, "carriage return is not a valid line ending");
			return lexer->error.code;
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

			make_layout_token(out_token, TOKEN_NEWLINE, location);
			return LEXER_RESULT_OK;
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
			token_location_t first_newline = {0};
			lexer_result_t result = skip_block_comment(lexer, &contains_newline, &first_newline);
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

			make_layout_token(out_token, TOKEN_NEWLINE, first_newline);
			return LEXER_RESULT_OK;
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

/* Gandalf the wizard of code
::::::--=---=++#@@@@@@%@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
::::::::---==+*%@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
::-------:::--=#%%%%%@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
:--------::::..=*##%#*##*#@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
::-----:-:::::::-=**+=--*@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@%%%@@@@@@@@@@@@@@@@@@@@@@@@@@@
:-=--:.::--------=++*@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@%#*==-==-:...   .....:::-==--=---=--=+#@@@@@@
--==---:-----==*@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@%%%**++==:..........   ..      .....:::::::::--------+@@
:-==-------+@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@%#**+=-====-------==--::............        ...::::::::::::::::-=
::==---*@@@@@@@@@@@@@@@@@@@@@@@@@%%**+====---==+***###+*****#%###*+=-:...........         ....:::::::::..::::
--=+@@@@@@@@@@@@@@@@@@%*%**+===-----:.........:-=+#%%@##%%#%@@#+-::.:............:..       ......::::::::...:
*@@@@@@@@@@@@@@@%*==--::--:=:.. ..--::.....:---:-==*#@*%%%##@%##%%%%#++=-::::.....:.         ........:::::::.
@@@@@@@@%*=-=--:..   ..::--......:-::::-=**###%@%%#**+#%%%%*++*#####%@%%#+::-:.:..... .     ........:::.:::::
%*+==--.            .:-==:.. ...::-:::-=***+-:--==+=--===+**===-...=:.:*%#=--:........         ........::....
.   .     . .        =+=:.. ..::::--:-++:.:-==----::::::::-======+*%%%#*+#+---:...::..     .  . .............
                   .==+-... ....----:-===#*##%%#*+--:::--=+++****#%%%@%%%#*===-........        . ............
            .     .-=+=:...   ..:--:--++==+****+==-----:-=++*#**####%%%%#%%*+=....::::..         ............
  . .            .:--=-:..    ...--:-=++*****++=---=-::::=++=*#######%%@@%%#**=:....:-..            ....  ..
                .:-.--:.      ...:--=+****++++++**-:--:-=+***+*+#@@%%%%@@%##**+==-...:-.       .   .....   ..
     .         .::::::..   ......:-=+****+***##%#::::-=+**#%%#*++*@@@@@@@%####**++=:.--:.       ......
 .             :::..:...  ........-=+****##%%%%#=-**++*#%%%@@%##%%%@@@@@%%%%####***=.:=-.     ....  ...
              :-:..:...  .........:-+****#%@@@%*++*#####%@@@@%%@@@@@@@@@@%####**+***--=-:.     .. .....    .
             .=:..:.... ..........:-+****##%%%#%##****##%@@@@@@@@@@@@@@@@%%%%%#*+***--=--.. . .        .....=
   .         =:.......  ..:::......:-=+***#%%%%%%%%%%%##%%@@@@@@@@@@@@@@%@%@###**#%+=-=+=:       .   ...:+***
    .      .-:........  .:::::::....:-=+**%@%%@@@@@@@@@%%@@@@%@@@@@@@@@@@@@%**####%#+=**-.         .:#%*+****
:         .::.....  .  .::::=--:....:--=*#@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@#**+**%%%%%#+-:. . ..:**-:--=**#%%
=+=-:.. ..:::.....   ..:-:---:..::..:::=*#@@@@@@#=*%@@@@%%@@@@@@@@@@@@@@%%%%**###*#%@@%*+-.:+@@#*:.:-=+***###
=-=--=+#*+=-::...... ......:-:.::::----+#%@@@@@@%*:  .:--::::. :#@@@@@@@@#*+++**##%%@@@%%@@@#++*####**#*+++++
=++=+**%##**=-:.......:...---:--===---=+#@@@@@@@%#*=-=--=-:-=*#%@@@@@@@@@@#*+++*%@@%@@%@@@@@@*=-========-----
*##**+*%%#*=----:::.-.:..:=+-=======-=+#@@@@@@@%#%#######%%%@@@@@@@@@@@@@@@#**#%@@@@@@@@@@@@@@*---====+==---:
%*######**=-::--:--::=:.:-*=-=++++*+=+*#%@@@@@#%####*%%%%%%%@@@@@@@@@@@@@%%%%%%@@@@%@@@@@@@@@@@+=::---------:
@%%%%#=+=+==. -::::-----+*+-==***=****##%@@@@@@%%%##****#%%%%@@%%%@@%#@@@@@@%@@@@@%%@%@@@@@@@@@@+-::...--:...
%%@=--+=++-:   :.::.+++**=====*#+##%%####%@%@@@@%##*++++++*%%%%#%#%%*#@@@@@@@@@@@@@@@@%@@@@@@@@##+::...:-----
%#--======-:  ..:...=+*#+====+*#####***#%##*#@@@%#%#+=::-=+####%%####*@@@@@@@@@@@@@@@@@@@@@@@@@@%*=:::::.....
=:::--+=-::.. ......:+#*===++***#%+#####%%%#@@@@%@#%#**+*#%%#@@@@@@%%%@@@@@@@@@@@@@@@@@@@@@@@@@@%##*=-:::.  .
.....:==--:.. .......:*+++++*##*#**#*####%%%@@@@@%%%%%#%%%%@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@#*+--::::.:
*/