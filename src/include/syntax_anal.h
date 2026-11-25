/**
 * @file syntax_anal.h
 * @author xkohela00
 * @brief IFJ26 syntax analyser interface.
 *
 * Statements and definitions are parsed by recursive descent, expressions by a precedence table.
 * While parsing, the parser reports what it recognized to the semantic analyser through
 * syntax_semantic_action().
 */

#ifndef IFJ_SYNTAX_ANAL_H
#define IFJ_SYNTAX_ANAL_H

#include <stdbool.h>
#include <stddef.h>

#include "lexer.h"
#include "sem_types.h"

/**
 * @brief Parser state shared by the recursive descent and the expression parser.
 *
 * After a failure the parser stops. The fields tell main which exit code to use: semantic_result
 * when it is not SEM_OK, 99 for internal_error, the lexer error from lexer_error() if there is one,
 * otherwise a syntax error 2.
 */
typedef struct SyntaxParser {
	lexer_t *lexer;               // Lexer that gives the current and the next token, not owned.
	bool failed;                  // Set after any error, parsing stops then.
	bool internal_error;          // Set together with failed when the parser runs out of memory (99).
	sem_result_t semantic_result; // Exit code from the semantic analyser, SEM_OK without error.
} syntax_parser_t;

/**
 * @brief Events the parser sends to the semantic analyser when it recognizes something.
 *
 * Events come in postfix order, the parts of a construct are reported before the construct itself,
 * e.g. f(1) + x gives PRIMARY f, PRIMARY 1, CALL 1, PRIMARY x, BINARY +.
 *
 * The token is NULL unless the event says otherwise. item_count is 0 unless the event says otherwise.
 */
typedef enum SyntaxSemanticAction {
	SEMANTIC_ACTION_PRIMARY, // Identifier or literal, token is the operand.
	SEMANTIC_ACTION_UNIT,    // Unit literal (), token is its opening parenthesis.
	SEMANTIC_ACTION_UNARY,   // Unary operator after its operand, token type is the operator.
	SEMANTIC_ACTION_BINARY,  // Binary operator after both operands, token type is the operator.
	SEMANTIC_ACTION_MEMBER,  // value.member after the value, token is the member name.
	SEMANTIC_ACTION_CALL,    // Call after the callee and the arguments, item_count is the argument count.
	SEMANTIC_ACTION_TYPE,    // Type of the last parameter, variable or function header, token is the type.
	SEMANTIC_ACTION_PROGRAM_BEGIN,     // Start of the program, before the prolog.
	SEMANTIC_ACTION_PROGRAM,           // Whole program parsed, the semantic analysis runs now.
	SEMANTIC_ACTION_FUNCTION,          // Function definition after its body.
	SEMANTIC_ACTION_FUNCTION_BEGIN,    // Token is def, item_count is 1 with @main and 0 without.
	SEMANTIC_ACTION_FUNCTION_NAME,     // Function name, parameters and the return type follow.
	SEMANTIC_ACTION_PARAMETER,         // Parameter name is the token, its TYPE comes next.
	SEMANTIC_ACTION_DECLARATION,       // val or var definition after its initializer.
	SEMANTIC_ACTION_DECLARATION_BEGIN, // val or var keyword is the token.
	SEMANTIC_ACTION_DECLARATION_NAME,  // Variable name, an optional TYPE and the initializer follow.
	SEMANTIC_ACTION_ASSIGNMENT,        // Assignment after the target and the value.
	/**
	 * Name at the start of a statement, token is the name. If no ASSIGNMENT follows, it is the first
	 * operand of an expression statement.
	 */
	SEMANTIC_ACTION_ASSIGNMENT_TARGET,
	SEMANTIC_ACTION_RETURN,       // return statement, item_count is 1 with a value and 0 without.
	SEMANTIC_ACTION_WHILE,        // while loop after the condition and the body.
	SEMANTIC_ACTION_FOR,          // for loop after the range, guards and body, item_count is the guard count.
	SEMANTIC_ACTION_FOR_VARIABLE, // Loop variable is the token, the start of the range follows.
	SEMANTIC_ACTION_RANGE_BEGIN,  // Start of the range is done, token is to or until.
	SEMANTIC_ACTION_RANGE,        // Range end done, item_count is 1, or 2 when a step with by follows.
	SEMANTIC_ACTION_IF,           // if after its parts, item_count is 2 without else and 3 with else.
	SEMANTIC_ACTION_BLOCK,        // End of an indented block.
	SEMANTIC_ACTION_BLOCK_BEGIN   // Start of an indented block, its statements follow.
} syntax_semantic_action_t;

/**
 * @brief Sends one event to the semantic analyser, defined in semantic_anal.c.
 *
 * @param action What was recognized.
 * @param token Token of it, NULL if there is none. It is only borrowed for the call.
 * @param item_count Number described at the event, 0 for the others.
 * @return SEM_OK, or the exit code of an error, parsing stops then.
 */
sem_result_t syntax_semantic_action(syntax_semantic_action_t action, const token_t *token, size_t item_count);

/**
 * @brief Initializes the parser state around an initialized lexer.
 *
 * @param[out] parser Parser to initialize.
 * @param lexer Lexer that already has its current and next token.
 * @return true on success, false when an argument is NULL or the lexer has no tokens.
 */
bool syntax_parser_init(syntax_parser_t *parser, lexer_t *lexer);

/**
 * @brief Returns the current token.
 *
 * @param parser Parser to read from.
 * @return Borrowed token, valid until the next advance, or NULL if parsing has failed.
 */
const token_t *syntax_parser_current(const syntax_parser_t *parser);

/**
 * @brief Returns the lookahead token after the current one.
 *
 * @param parser Parser to read from.
 * @return Borrowed token, valid until the next advance, or NULL if parsing has failed.
 */
const token_t *syntax_parser_next(const syntax_parser_t *parser);

/**
 * @brief Consumes the current token and moves the lexer forward.
 *
 * @param parser Parser to advance, marked failed on a lexer error.
 * @return true on success, false on error.
 */
bool syntax_parser_advance(syntax_parser_t *parser);

/**
 * @brief Requires and consumes a token of the given type.
 *
 * @param parser Parser to advance, marked failed on a mismatch.
 * @param expected Required token type.
 * @return true when the token matched and was consumed, false otherwise.
 */
bool syntax_parser_expect(syntax_parser_t *parser, token_type_t expected);

/**
 * @brief Consumes the current token only if it has the given type.
 *
 * @param parser Parser to advance.
 * @param expected Token type that is accepted.
 * @param[out] accepted Set to true when the token matched and was consumed, otherwise the input stays.
 * @return true on success (matched or not), false on error.
 */
bool syntax_parser_accept(syntax_parser_t *parser, token_type_t expected, bool *accepted);

/**
 * @brief Parses one expression with the precedence table.
 *
 * Delimiters after the expression (newline, comma, closing parenthesis) are left for the caller.
 *
 * @param parser Parser sitting on the first token of the expression.
 * @return true on success, false on error.
 */
bool syntax_parse_expression(syntax_parser_t *parser);

/**
 * @brief Parses a complete program: the prolog, the function definitions and EOF.
 *
 * At the end the semantic analysis runs through SEMANTIC_ACTION_PROGRAM.
 *
 * @param parser Initialized parser.
 * @return true when the program is valid, false on any error (see syntax_parser_t for the exit code).
 */
bool syntax_parse_program(syntax_parser_t *parser);

#endif /* IFJ_SYNTAX_ANAL_H */
