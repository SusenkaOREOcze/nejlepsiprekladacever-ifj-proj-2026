#ifndef IFJ_SYNTAX_ANAL_H
#define IFJ_SYNTAX_ANAL_H

#include <stdbool.h>
#include <stddef.h>

#include "lexer.h"

/** Parser state shared by the recursive-descent routines. */
typedef struct SyntaxParser {
	lexer_t *lexer; // lexer that gives us the current and next token
	/** Set after a syntax or lexer failure; parsing then stops. */
	bool failed;

	// set together with failed when the parser itself runs out of memory, exit code 99
	// so main can tell it apart from a syntax error 2 or a lexer error, see lexer_error
	bool internal_error;
} syntax_parser_t;

/**
 * @brief events the parser would send to the semantic analyser when it recognizes something
 */
typedef enum SyntaxSemanticAction {
	SEMANTIC_ACTION_PRIMARY,
	SEMANTIC_ACTION_UNIT,
	SEMANTIC_ACTION_UNARY,
	SEMANTIC_ACTION_BINARY,
	SEMANTIC_ACTION_MEMBER,
	SEMANTIC_ACTION_CALL,
	SEMANTIC_ACTION_TYPE,
	SEMANTIC_ACTION_PROGRAM,
	SEMANTIC_ACTION_FUNCTION,
	SEMANTIC_ACTION_FUNCTION_BEGIN,
	SEMANTIC_ACTION_FUNCTION_NAME,
	SEMANTIC_ACTION_PARAMETER,
	SEMANTIC_ACTION_DECLARATION,
	SEMANTIC_ACTION_DECLARATION_BEGIN,
	SEMANTIC_ACTION_DECLARATION_NAME,
	SEMANTIC_ACTION_ASSIGNMENT,
	SEMANTIC_ACTION_ASSIGNMENT_TARGET,
	SEMANTIC_ACTION_RETURN,
	SEMANTIC_ACTION_WHILE,
	SEMANTIC_ACTION_FOR,
	SEMANTIC_ACTION_IF,
	SEMANTIC_ACTION_BLOCK,
	SEMANTIC_ACTION_BLOCK_BEGIN
} syntax_semantic_action_t;

/**
 * @brief placeholder for the semantic analyser dispatcher
 *
 * @param action what was recognized
 * @param token token of it, NULL if there isnt one
 * @param item_count argument count for SEMANTIC_ACTION_CALL, 0 otherwise
 */
void syntax_semantic_action(syntax_semantic_action_t action, const token_t *token, size_t item_count);

/** Initialize parser state around an initialized lexer. Returns false for invalid input. */
bool syntax_parser_init(syntax_parser_t *parser, lexer_t *lexer);

/** Return borrowed current/lookahead tokens, or NULL if parsing has failed. */
const token_t *syntax_parser_current(const syntax_parser_t *parser);
const token_t *syntax_parser_next(const syntax_parser_t *parser);

/** Consume the current token and advance the lexer; marks the parser failed on lexer error. */
bool syntax_parser_advance(syntax_parser_t *parser);

/** Require and consume a token of the given type; a mismatch marks parsing failed. */
bool syntax_parser_expect(syntax_parser_t *parser, token_type_t expected);

/** Consume a matching token and set accepted; otherwise leave input unchanged. */
bool syntax_parser_accept(syntax_parser_t *parser, token_type_t expected, bool *accepted);

/** Parse one expression using the precedence groups from the precedence table. */
bool syntax_parse_expression(syntax_parser_t *parser);

/** Parse the prolog, definitions, and EOF for one complete program. */
bool syntax_parse_program(syntax_parser_t *parser);

#endif /* IFJ_SYNTAX_ANAL_H */
