/*
AI PROMPT:
Look at file "src/tests/test_syntax_anal.c" and the parser in "src/syntax_anal.c".
These cmocka tests use a fake lexer and semantic-action callback to focus on syntax.
They cover parser setup and token lookahead, lexer/parser errors, expressions and
precedence (including calls, members, if-expressions, nesting and malformed input),
statement forms, separators, indentation-based blocks, if/while/for syntax, function
headers and whole-program structure + allocation failures. Have a look through the
tests and edit/add anything needed so they check the parser behavior above.
 */
/**
 * @file test_syntax_anal.c
 * @author xkohela00
 * @brief Unit tests of the syntax analyser with a fake lexer, no other module is needed.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cmocka/cmocka.h"

#define lexer_current fake_lexer_current
#define lexer_next fake_lexer_next
#define lexer_advance fake_lexer_advance
#define lexer_error fake_lexer_error
#define realloc fake_realloc

#define syntax_parser_init test_syntax_parser_init
#define syntax_parser_current test_syntax_parser_current
#define syntax_parser_next test_syntax_parser_next
#define syntax_parser_advance test_syntax_parser_advance
#define syntax_parser_expect test_syntax_parser_expect
#define syntax_parser_accept test_syntax_parser_accept
#define syntax_parse_expression test_syntax_parse_expression
#define syntax_parse_program test_syntax_parse_program
#define syntax_semantic_action fake_semantic_action

static void *fake_realloc(void *pointer, size_t size);

#include "../syntax_anal.c"

#undef realloc

// these tests check syntax only, so every semantic action is accepted
sem_result_t fake_semantic_action(syntax_semantic_action_t action, const token_t *token, size_t item_count) {
	(void)action;
	(void)token;
	(void)item_count;
	return SEM_OK;
}

/*
  FAKE LEXER AND ALLOCATOR
*/

static const token_t *fake_tokens;  // token stream of the current test, always ends with EOF
static size_t fake_token_count;     // number of tokens in fake_tokens
static size_t fake_position;        // index of the current token
static size_t fake_failing_advance; // which lexer_advance call fails (1 is the first), 0 means none
static size_t fake_advance_calls;   // how many times lexer_advance was called
static size_t fake_realloc_budget;  // how many more reallocs succeed, SIZE_MAX means all of them

const token_t *fake_lexer_current(const lexer_t *lexer) {
	(void)lexer;
	if (fake_tokens == NULL || fake_token_count == 0) {
		return NULL;
	}
	return &fake_tokens[fake_position];
}

const token_t *fake_lexer_next(const lexer_t *lexer) {
	(void)lexer;
	if (fake_tokens == NULL || fake_token_count == 0) {
		return NULL;
	}
	// past the end the real lexer keeps returning EOF, so stay on the last token
	size_t next = fake_position + 1 < fake_token_count ? fake_position + 1 : fake_token_count - 1;
	return &fake_tokens[next];
}

lexer_result_t fake_lexer_advance(lexer_t *lexer) {
	(void)lexer;
	fake_advance_calls++;
	if (fake_tokens == NULL || fake_token_count == 0) {
		return LEXER_RESULT_INVALID_ARGUMENT;
	}
	if (fake_advance_calls == fake_failing_advance) {
		return LEXER_RESULT_LEXICAL_ERROR;
	}
	if (fake_position + 1 < fake_token_count) {
		fake_position++;
	}
	return LEXER_RESULT_OK;
}

// like the real lexer, the error stays reported once the failing advance happened
const lexer_error_t *fake_lexer_error(const lexer_t *lexer) {
	(void)lexer;
	static const lexer_error_t error = {.code = LEXER_RESULT_LEXICAL_ERROR, .message = "fake lexical error"};
	return fake_failing_advance != 0 && fake_advance_calls >= fake_failing_advance ? &error : NULL;
}

static void *fake_realloc(void *pointer, size_t size) {
	if (fake_realloc_budget == 0) {
		return NULL;
	}
	if (fake_realloc_budget != SIZE_MAX) {
		fake_realloc_budget--;
	}
	return realloc(pointer, size);
}

/*
  HELPERS
*/

// token without text, eg operators keywords and layout tokens
#define T(kind) {.type = (kind), .location = {1, 1}, .lexeme = NULL, .length = 0}
// token without text on a given line, needed for the trailing comma rule
#define T_AT(kind, line) {.type = (kind), .location = {(line), 1}, .lexeme = NULL, .length = 0}
// token with text, eg identifiers and literals
#define TX(kind, text)                                                                                       \
	{.type = (kind), .location = {1, 1}, .lexeme = (char *)(text), .length = sizeof(text) - 1}

#define ID(text) TX(TOKEN_IDENTIFIER, text)
#define INT(text) TX(TOKEN_INTEGER, text)
#define NL T(TOKEN_NEWLINE)
#define INDENT T(TOKEN_INDENT)
#define DEDENT T(TOKEN_DEDENT)

// import ifj26.*
#define PROLOG T(TOKEN_IMPORT), ID("ifj26"), T(TOKEN_DOT), T(TOKEN_MULTIPLY), NL
// @main def main(): Unit =  followed by an indented block
#define MAIN_HEAD                                                                                            \
	T(TOKEN_MAIN_ANNOTATION), T(TOKEN_DEF), ID("main"), T(TOKEN_LPAREN), T(TOKEN_RPAREN), T(TOKEN_COLON),    \
	    T(TOKEN_TYPE_UNIT), T(TOKEN_ASSIGN), NL, INDENT

// builds the token array with EOF at the end and loads it into the fake lexer and the parser
#define TOKEN_ARRAY(...) ((const token_t[]){__VA_ARGS__, T(TOKEN_EOF)})
#define LOAD(...) load_tokens(TOKEN_ARRAY(__VA_ARGS__), sizeof(TOKEN_ARRAY(__VA_ARGS__)) / sizeof(token_t))
// whole program with only a main function whose body are the given tokens
#define LOAD_MAIN(...) LOAD(PROLOG, MAIN_HEAD, __VA_ARGS__, DEDENT)

static lexer_t dummy_lexer; // never read, the fake lexer ignores it
static syntax_parser_t parser;

/** Resets the fakes and starts the parser on the given tokens. */
static void load_tokens(const token_t *tokens, size_t count) {
	fake_tokens = tokens;
	fake_token_count = count;
	fake_position = 0;
	fake_failing_advance = 0;
	fake_advance_calls = 0;
	fake_realloc_budget = SIZE_MAX;
	parser = (syntax_parser_t){0};
	assert_true(syntax_parser_init(&parser, &dummy_lexer));
}

/** Type of the token the parser is sitting on. */
static token_type_t current_type(void) {
	const token_t *token = syntax_parser_current(&parser);
	assert_non_null(token);
	return token->type;
}

/** Checks that the expression was accepted and the parser stopped on the expected token. */
static void assert_expression_stops_at(token_type_t stop) {
	assert_true(syntax_parse_expression(&parser));
	assert_false(parser.failed);
	assert_int_equal(current_type(), stop);
}

/** Checks a syntax error, which is not an internal error. */
static void assert_syntax_error(bool result) {
	assert_false(result);
	assert_true(parser.failed);
	assert_false(parser.internal_error);
}

/** Checks that the whole program was accepted. */
static void assert_program_ok(void) {
	assert_true(syntax_parse_program(&parser));
	assert_false(parser.failed);
	assert_false(parser.internal_error);
}

/** Checks that the program was rejected with a syntax error. */
static void assert_program_rejected(void) { assert_syntax_error(syntax_parse_program(&parser)); }

#define EXPRESSION_OK(stop, ...)                                                                             \
	do {                                                                                                     \
		LOAD(__VA_ARGS__);                                                                                   \
		assert_expression_stops_at(stop);                                                                    \
	} while (0)

#define EXPRESSION_REJECTED(...)                                                                             \
	do {                                                                                                     \
		LOAD(__VA_ARGS__);                                                                                   \
		assert_syntax_error(syntax_parse_expression(&parser));                                               \
	} while (0)

#define MAIN_OK(...)                                                                                         \
	do {                                                                                                     \
		LOAD_MAIN(__VA_ARGS__);                                                                              \
		assert_program_ok();                                                                                 \
	} while (0)

#define MAIN_REJECTED(...)                                                                                   \
	do {                                                                                                     \
		LOAD_MAIN(__VA_ARGS__);                                                                              \
		assert_program_rejected();                                                                           \
	} while (0)

#define PROGRAM_OK(...)                                                                                      \
	do {                                                                                                     \
		LOAD(__VA_ARGS__);                                                                                   \
		assert_program_ok();                                                                                 \
	} while (0)

#define PROGRAM_REJECTED(...)                                                                                \
	do {                                                                                                     \
		LOAD(__VA_ARGS__);                                                                                   \
		assert_program_rejected();                                                                           \
	} while (0)

#define EXEC_TEST_CATEGORY(name, tests)                                                                      \
	do {                                                                                                     \
		printf("\n[    " name "    ]\n");                                                                    \
		test_results |= cmocka_run_group_tests_name((name), (tests), NULL, NULL);                            \
	} while (0)

/*
  PARSER API
*/

static void init_rejects_null_arguments(void **state) {
	(void)state;
	LOAD(ID("x"));
	assert_false(syntax_parser_init(NULL, &dummy_lexer));
	assert_false(syntax_parser_init(&parser, NULL));
}

static void init_rejects_lexer_without_tokens(void **state) {
	(void)state;
	// lexer that failed to start has no current token
	fake_tokens = NULL;
	fake_token_count = 0;
	syntax_parser_t empty;
	assert_false(syntax_parser_init(&empty, &dummy_lexer));
}

static void init_resets_parser_state(void **state) {
	(void)state;
	LOAD(ID("x"), NL);
	assert_ptr_equal(parser.lexer, &dummy_lexer);
	assert_false(parser.failed);
	assert_false(parser.internal_error);
}

static void current_and_next_show_lookahead(void **state) {
	(void)state;
	LOAD(ID("x"), T(TOKEN_ASSIGN));
	assert_int_equal(syntax_parser_current(&parser)->type, TOKEN_IDENTIFIER);
	assert_int_equal(syntax_parser_next(&parser)->type, TOKEN_ASSIGN);
	assert_true(syntax_parser_advance(&parser));
	assert_int_equal(syntax_parser_current(&parser)->type, TOKEN_ASSIGN);
	assert_int_equal(syntax_parser_next(&parser)->type, TOKEN_EOF);
}

static void current_and_next_handle_null_parser(void **state) {
	(void)state;
	assert_null(syntax_parser_current(NULL));
	assert_null(syntax_parser_next(NULL));
	assert_false(syntax_parser_advance(NULL));
	assert_false(syntax_parser_expect(NULL, TOKEN_EOF));
	bool accepted = false;
	assert_false(syntax_parser_accept(NULL, TOKEN_EOF, &accepted));
	assert_false(syntax_parse_expression(NULL));
	assert_false(syntax_parse_program(NULL));
}

static void expect_consumes_matching_token(void **state) {
	(void)state;
	LOAD(T(TOKEN_LPAREN), T(TOKEN_RPAREN));
	assert_true(syntax_parser_expect(&parser, TOKEN_LPAREN));
	assert_int_equal(current_type(), TOKEN_RPAREN);
	assert_false(parser.failed);
}

static void expect_mismatch_fails_parser(void **state) {
	(void)state;
	LOAD(T(TOKEN_LPAREN), T(TOKEN_RPAREN));
	assert_false(syntax_parser_expect(&parser, TOKEN_RPAREN));
	assert_true(parser.failed);
	assert_int_equal(fake_advance_calls, 0);
}

static void failed_parser_hides_tokens_and_stops(void **state) {
	(void)state;
	LOAD(ID("x"), NL);
	assert_false(syntax_parser_expect(&parser, TOKEN_NEWLINE));
	// once failed nothing else is read
	assert_null(syntax_parser_current(&parser));
	assert_null(syntax_parser_next(&parser));
	assert_false(syntax_parser_advance(&parser));
	assert_false(syntax_parse_expression(&parser));
	assert_false(syntax_parse_program(&parser));
	assert_int_equal(fake_advance_calls, 0);
}

static void accept_consumes_only_matching_token(void **state) {
	(void)state;
	LOAD(T(TOKEN_COLON), ID("x"));
	bool accepted = true;
	assert_true(syntax_parser_accept(&parser, TOKEN_COMMA, &accepted));
	assert_false(accepted);
	assert_int_equal(current_type(), TOKEN_COLON);

	assert_true(syntax_parser_accept(&parser, TOKEN_COLON, &accepted));
	assert_true(accepted);
	assert_int_equal(current_type(), TOKEN_IDENTIFIER);
	assert_false(parser.failed);
}

static void accept_rejects_missing_result_pointer(void **state) {
	(void)state;
	LOAD(T(TOKEN_COLON));
	assert_false(syntax_parser_accept(&parser, TOKEN_COLON, NULL));
	assert_int_equal(fake_advance_calls, 0);
}

static void advance_reports_lexer_error(void **state) {
	(void)state;
	LOAD(ID("x"), NL);
	fake_failing_advance = 1;
	assert_false(syntax_parser_advance(&parser));
	assert_true(parser.failed);
	assert_false(parser.internal_error);
}

/*
  EXPRESSIONS ACCEPTED
*/

static void expression_accepts_single_operands(void **state) {
	(void)state;
	EXPRESSION_OK(TOKEN_NEWLINE, ID("x"), NL);
	EXPRESSION_OK(TOKEN_NEWLINE, INT("42"), NL);
	EXPRESSION_OK(TOKEN_NEWLINE, TX(TOKEN_DOUBLE, "3.14"), NL);
	EXPRESSION_OK(TOKEN_NEWLINE, TX(TOKEN_STRING, "hi"), NL);
	EXPRESSION_OK(TOKEN_NEWLINE, TX(TOKEN_MULTILINE_STRING, "hi"), NL);
	EXPRESSION_OK(TOKEN_NEWLINE, TX(TOKEN_TRUE, "true"), NL);
	EXPRESSION_OK(TOKEN_NEWLINE, TX(TOKEN_FALSE, "false"), NL);
	// () is the unit literal
	EXPRESSION_OK(TOKEN_NEWLINE, T(TOKEN_LPAREN), T(TOKEN_RPAREN), NL);
}

static void expression_accepts_arithmetic(void **state) {
	(void)state;
	// 1 + 2 * 3
	EXPRESSION_OK(TOKEN_NEWLINE, INT("1"), T(TOKEN_PLUS), INT("2"), T(TOKEN_MULTIPLY), INT("3"), NL);
	// a - b / c - d
	EXPRESSION_OK(TOKEN_NEWLINE, ID("a"), T(TOKEN_MINUS), ID("b"), T(TOKEN_DIVIDE), ID("c"), T(TOKEN_MINUS),
	              ID("d"), NL);
	// "a" + x
	EXPRESSION_OK(TOKEN_NEWLINE, TX(TOKEN_STRING, "a"), T(TOKEN_PLUS), ID("x"), NL);
}

static void expression_accepts_parentheses(void **state) {
	(void)state;
	// (1 + 2) * 3
	EXPRESSION_OK(TOKEN_NEWLINE, T(TOKEN_LPAREN), INT("1"), T(TOKEN_PLUS), INT("2"), T(TOKEN_RPAREN),
	              T(TOKEN_MULTIPLY), INT("3"), NL);
	// ((x))
	EXPRESSION_OK(TOKEN_NEWLINE, T(TOKEN_LPAREN), T(TOKEN_LPAREN), ID("x"), T(TOKEN_RPAREN), T(TOKEN_RPAREN),
	              NL);
	// a * (b - (c + d))
	EXPRESSION_OK(TOKEN_NEWLINE, ID("a"), T(TOKEN_MULTIPLY), T(TOKEN_LPAREN), ID("b"), T(TOKEN_MINUS),
	              T(TOKEN_LPAREN), ID("c"), T(TOKEN_PLUS), ID("d"), T(TOKEN_RPAREN), T(TOKEN_RPAREN), NL);
}

static void expression_accepts_unary_operators(void **state) {
	(void)state;
	// -x
	EXPRESSION_OK(TOKEN_NEWLINE, T(TOKEN_MINUS), ID("x"), NL);
	// +x
	EXPRESSION_OK(TOKEN_NEWLINE, T(TOKEN_PLUS), ID("x"), NL);
	// !b
	EXPRESSION_OK(TOKEN_NEWLINE, T(TOKEN_NOT), ID("b"), NL);
	// - - x
	EXPRESSION_OK(TOKEN_NEWLINE, T(TOKEN_MINUS), T(TOKEN_MINUS), ID("x"), NL);
	// a * -b
	EXPRESSION_OK(TOKEN_NEWLINE, ID("a"), T(TOKEN_MULTIPLY), T(TOKEN_MINUS), ID("b"), NL);
	// -(a + b)
	EXPRESSION_OK(TOKEN_NEWLINE, T(TOKEN_MINUS), T(TOKEN_LPAREN), ID("a"), T(TOKEN_PLUS), ID("b"),
	              T(TOKEN_RPAREN), NL);
	// !a && b
	EXPRESSION_OK(TOKEN_NEWLINE, T(TOKEN_NOT), ID("a"), T(TOKEN_AND), ID("b"), NL);
}

static void expression_accepts_relational_and_logical(void **state) {
	(void)state;
	// a < b == c >= d
	EXPRESSION_OK(TOKEN_NEWLINE, ID("a"), T(TOKEN_LESS), ID("b"), T(TOKEN_EQUAL), ID("c"),
	              T(TOKEN_GREATER_EQUAL), ID("d"), NL);
	// a <= b && c > d || !e
	EXPRESSION_OK(TOKEN_NEWLINE, ID("a"), T(TOKEN_LESS_EQUAL), ID("b"), T(TOKEN_AND), ID("c"),
	              T(TOKEN_GREATER), ID("d"), T(TOKEN_OR), T(TOKEN_NOT), ID("e"), NL);
	// a != b + 1
	EXPRESSION_OK(TOKEN_NEWLINE, ID("a"), T(TOKEN_NOT_EQUAL), ID("b"), T(TOKEN_PLUS), INT("1"), NL);
	// (a < b) == (c < d), parentheses make the chain legal
	EXPRESSION_OK(TOKEN_NEWLINE, T(TOKEN_LPAREN), ID("a"), T(TOKEN_LESS), ID("b"), T(TOKEN_RPAREN),
	              T(TOKEN_EQUAL), T(TOKEN_LPAREN), ID("c"), T(TOKEN_LESS), ID("d"), T(TOKEN_RPAREN), NL);
}

static void expression_accepts_calls(void **state) {
	(void)state;
	// f()
	EXPRESSION_OK(TOKEN_NEWLINE, ID("f"), T(TOKEN_LPAREN), T(TOKEN_RPAREN), NL);
	// f(a)
	EXPRESSION_OK(TOKEN_NEWLINE, ID("f"), T(TOKEN_LPAREN), ID("a"), T(TOKEN_RPAREN), NL);
	// f(a, 1, "s")
	EXPRESSION_OK(TOKEN_NEWLINE, ID("f"), T(TOKEN_LPAREN), ID("a"), T(TOKEN_COMMA), INT("1"), T(TOKEN_COMMA),
	              TX(TOKEN_STRING, "s"), T(TOKEN_RPAREN), NL);
	// f(g(h(1)), x + 1, -y)
	EXPRESSION_OK(TOKEN_NEWLINE, ID("f"), T(TOKEN_LPAREN), ID("g"), T(TOKEN_LPAREN), ID("h"), T(TOKEN_LPAREN),
	              INT("1"), T(TOKEN_RPAREN), T(TOKEN_RPAREN), T(TOKEN_COMMA), ID("x"), T(TOKEN_PLUS),
	              INT("1"), T(TOKEN_COMMA), T(TOKEN_MINUS), ID("y"), T(TOKEN_RPAREN), NL);
	// 1 + f(2) * 3
	EXPRESSION_OK(TOKEN_NEWLINE, INT("1"), T(TOKEN_PLUS), ID("f"), T(TOKEN_LPAREN), INT("2"), T(TOKEN_RPAREN),
	              T(TOKEN_MULTIPLY), INT("3"), NL);
}

static void expression_accepts_trailing_comma_before_line_break(void **state) {
	(void)state;
	/* f(a,
	     b,
	   ) */
	EXPRESSION_OK(TOKEN_NEWLINE, ID("f"), T(TOKEN_LPAREN), ID("a"), T(TOKEN_COMMA), ID("b"), T(TOKEN_COMMA),
	              T_AT(TOKEN_RPAREN, 3), NL);
}

static void expression_accepts_members(void **state) {
	(void)state;
	// s.length
	EXPRESSION_OK(TOKEN_NEWLINE, ID("s"), T(TOKEN_DOT), ID("length"), NL);
	// x.floor.toInt
	EXPRESSION_OK(TOKEN_NEWLINE, ID("x"), T(TOKEN_DOT), ID("floor"), T(TOKEN_DOT), ID("toInt"), NL);
	// "abc".length + 1
	EXPRESSION_OK(TOKEN_NEWLINE, TX(TOKEN_STRING, "abc"), T(TOKEN_DOT), ID("length"), T(TOKEN_PLUS), INT("1"),
	              NL);
	// f(x).toString
	EXPRESSION_OK(TOKEN_NEWLINE, ID("f"), T(TOKEN_LPAREN), ID("x"), T(TOKEN_RPAREN), T(TOKEN_DOT),
	              ID("toString"), NL);
	// (a + b).toString
	EXPRESSION_OK(TOKEN_NEWLINE, T(TOKEN_LPAREN), ID("a"), T(TOKEN_PLUS), ID("b"), T(TOKEN_RPAREN),
	              T(TOKEN_DOT), ID("toString"), NL);
	// -x.length, member binds tighter than unary minus
	EXPRESSION_OK(TOKEN_NEWLINE, T(TOKEN_MINUS), ID("x"), T(TOKEN_DOT), ID("length"), NL);
}

static void expression_accepts_if_expression(void **state) {
	(void)state;
	// if a then 1 else 2
	EXPRESSION_OK(TOKEN_NEWLINE, T(TOKEN_IF), ID("a"), T(TOKEN_THEN), INT("1"), T(TOKEN_ELSE), INT("2"), NL);
	// x + (if a < b then a else b)
	EXPRESSION_OK(TOKEN_NEWLINE, ID("x"), T(TOKEN_PLUS), T(TOKEN_LPAREN), T(TOKEN_IF), ID("a"), T(TOKEN_LESS),
	              ID("b"), T(TOKEN_THEN), ID("a"), T(TOKEN_ELSE), ID("b"), T(TOKEN_RPAREN), NL);
	// if a then 1 else if b then 2 else 3
	EXPRESSION_OK(TOKEN_NEWLINE, T(TOKEN_IF), ID("a"), T(TOKEN_THEN), INT("1"), T(TOKEN_ELSE), T(TOKEN_IF),
	              ID("b"), T(TOKEN_THEN), INT("2"), T(TOKEN_ELSE), INT("3"), NL);
}

static void expression_accepts_deep_nesting(void **state) {
	(void)state;
	// 100 nested parentheses around x, makes the precedence stack grow a few times
	enum { DEPTH = 100 };
	static token_t tokens[2 * DEPTH + 3];
	size_t count = 0;
	for (size_t i = 0; i < DEPTH; i++) {
		tokens[count++] = (token_t)T(TOKEN_LPAREN);
	}
	tokens[count++] = (token_t)ID("x");
	for (size_t i = 0; i < DEPTH; i++) {
		tokens[count++] = (token_t)T(TOKEN_RPAREN);
	}
	tokens[count++] = (token_t)NL;
	tokens[count++] = (token_t)T(TOKEN_EOF);
	load_tokens(tokens, count);
	assert_expression_stops_at(TOKEN_NEWLINE);
}

static void expression_accepts_long_operator_chain(void **state) {
	(void)state;
	// 1 + 1 + ... + 1 with 200 operands
	enum { OPERANDS = 200 };
	static token_t tokens[2 * OPERANDS + 1];
	size_t count = 0;
	for (size_t i = 0; i < OPERANDS; i++) {
		if (i > 0) {
			tokens[count++] = (token_t)T(TOKEN_PLUS);
		}
		tokens[count++] = (token_t)INT("1");
	}
	tokens[count++] = (token_t)NL;
	tokens[count++] = (token_t)T(TOKEN_EOF);
	load_tokens(tokens, count);
	assert_expression_stops_at(TOKEN_NEWLINE);
}

/*===============
  WHERE EXPRESSIONS STOP
  ===============*/

static void expression_leaves_terminators_for_caller(void **state) {
	(void)state;
	EXPRESSION_OK(TOKEN_NEWLINE, ID("a"), T(TOKEN_PLUS), ID("b"), NL);
	EXPRESSION_OK(TOKEN_SEMICOLON, ID("a"), T(TOKEN_SEMICOLON), NL);
	EXPRESSION_OK(TOKEN_DEDENT, ID("a"), DEDENT);
	EXPRESSION_OK(TOKEN_EOF, ID("a"));
}

static void expression_leaves_keywords_for_caller(void **state) {
	(void)state;
	// if a then ...
	EXPRESSION_OK(TOKEN_THEN, ID("a"), T(TOKEN_THEN));
	// while a do ...
	EXPRESSION_OK(TOKEN_DO, ID("a"), T(TOKEN_LESS), ID("b"), T(TOKEN_DO));
	// ... then 1 else ...
	EXPRESSION_OK(TOKEN_ELSE, INT("1"), T(TOKEN_ELSE));
	// for i <- 1 to 5, to is parsed by the for loop, not as a range expression
	EXPRESSION_OK(TOKEN_IDENTIFIER, INT("1"), ID("to"), INT("5"));
	// for ... if guard, if after an operand is a guard
	EXPRESSION_OK(TOKEN_IF, ID("a"), T(TOKEN_IF), ID("b"));
}

static void expression_leaves_delimiters_for_caller(void **state) {
	(void)state;
	// close paren that closes an outer argument list
	EXPRESSION_OK(TOKEN_RPAREN, ID("a"), T(TOKEN_PLUS), INT("1"), T(TOKEN_RPAREN));
	EXPRESSION_OK(TOKEN_COMMA, ID("a"), T(TOKEN_COMMA), ID("b"));
	EXPRESSION_OK(TOKEN_ASSIGN, ID("a"), T(TOKEN_ASSIGN), ID("b"));
	EXPRESSION_OK(TOKEN_COLON, ID("a"), T(TOKEN_COLON));
	EXPRESSION_OK(TOKEN_RBRACKET, ID("a"), T(TOKEN_RBRACKET));
}

static void expression_stops_at_operand_after_operand(void **state) {
	(void)state;
	// x y, the caller decides that this is an error
	EXPRESSION_OK(TOKEN_IDENTIFIER, ID("x"), ID("y"), NL);
	// x 5
	EXPRESSION_OK(TOKEN_INTEGER, ID("x"), INT("5"), NL);
}

/*
  EXPRESSIONS REJECTED
*/

static void expression_rejects_empty_input(void **state) {
	(void)state;
	EXPRESSION_REJECTED(NL);
	EXPRESSION_REJECTED(T(TOKEN_RPAREN));
	EXPRESSION_REJECTED(T(TOKEN_THEN));
}

static void expression_rejects_missing_operands(void **state) {
	(void)state;
	// 1 +
	EXPRESSION_REJECTED(INT("1"), T(TOKEN_PLUS), NL);
	// * 2
	EXPRESSION_REJECTED(T(TOKEN_MULTIPLY), INT("2"), NL);
	// 1 * / 2
	EXPRESSION_REJECTED(INT("1"), T(TOKEN_MULTIPLY), T(TOKEN_DIVIDE), INT("2"), NL);
	// -
	EXPRESSION_REJECTED(T(TOKEN_MINUS), NL);
	// a &&
	EXPRESSION_REJECTED(ID("a"), T(TOKEN_AND), NL);
	// !
	EXPRESSION_REJECTED(T(TOKEN_NOT), NL);
}

static void expression_rejects_chained_relations(void **state) {
	(void)state;
	// 1 < 2 < 3
	EXPRESSION_REJECTED(INT("1"), T(TOKEN_LESS), INT("2"), T(TOKEN_LESS), INT("3"), NL);
	// a <= b >= c
	EXPRESSION_REJECTED(ID("a"), T(TOKEN_LESS_EQUAL), ID("b"), T(TOKEN_GREATER_EQUAL), ID("c"), NL);
	// 1 == 2 != 3
	EXPRESSION_REJECTED(INT("1"), T(TOKEN_EQUAL), INT("2"), T(TOKEN_NOT_EQUAL), INT("3"), NL);
	// a == b == c
	EXPRESSION_REJECTED(ID("a"), T(TOKEN_EQUAL), ID("b"), T(TOKEN_EQUAL), ID("c"), NL);
}

static void expression_rejects_unbalanced_parentheses(void **state) {
	(void)state;
	// (1 + 2
	EXPRESSION_REJECTED(T(TOKEN_LPAREN), INT("1"), T(TOKEN_PLUS), INT("2"), NL);
	// ((x)
	EXPRESSION_REJECTED(T(TOKEN_LPAREN), T(TOKEN_LPAREN), ID("x"), T(TOKEN_RPAREN), NL);
	// (+)
	EXPRESSION_REJECTED(T(TOKEN_LPAREN), T(TOKEN_PLUS), T(TOKEN_RPAREN), NL);
}

static void expression_rejects_bad_calls(void **state) {
	(void)state;
	// f(1
	EXPRESSION_REJECTED(ID("f"), T(TOKEN_LPAREN), INT("1"), NL);
	// f(1 2)
	EXPRESSION_REJECTED(ID("f"), T(TOKEN_LPAREN), INT("1"), INT("2"), T(TOKEN_RPAREN), NL);
	// f(,1)
	EXPRESSION_REJECTED(ID("f"), T(TOKEN_LPAREN), T(TOKEN_COMMA), INT("1"), T(TOKEN_RPAREN), NL);
	// f(1,,2)
	EXPRESSION_REJECTED(ID("f"), T(TOKEN_LPAREN), INT("1"), T(TOKEN_COMMA), T(TOKEN_COMMA), INT("2"),
	                    T(TOKEN_RPAREN), NL);
	// f(1,) on one line, trailing comma needs a line break after it
	EXPRESSION_REJECTED(ID("f"), T(TOKEN_LPAREN), INT("1"), T(TOKEN_COMMA), T(TOKEN_RPAREN), NL);
	// f(1 +)
	EXPRESSION_REJECTED(ID("f"), T(TOKEN_LPAREN), INT("1"), T(TOKEN_PLUS), T(TOKEN_RPAREN), NL);
}

static void expression_rejects_bad_members(void **state) {
	(void)state;
	// x.
	EXPRESSION_REJECTED(ID("x"), T(TOKEN_DOT), NL);
	// x.5
	EXPRESSION_REJECTED(ID("x"), T(TOKEN_DOT), INT("5"), NL);
	// x.(y)
	EXPRESSION_REJECTED(ID("x"), T(TOKEN_DOT), T(TOKEN_LPAREN), ID("y"), T(TOKEN_RPAREN), NL);
}

static void expression_rejects_incomplete_if_expression(void **state) {
	(void)state;
	// x + (if a then 1)
	EXPRESSION_REJECTED(ID("x"), T(TOKEN_PLUS), T(TOKEN_LPAREN), T(TOKEN_IF), ID("a"), T(TOKEN_THEN),
	                    INT("1"), T(TOKEN_RPAREN), NL);
	// if a 1 else 2
	EXPRESSION_REJECTED(T(TOKEN_IF), ID("a"), INT("1"), T(TOKEN_ELSE), INT("2"), NL);
	// if a then else 2
	EXPRESSION_REJECTED(T(TOKEN_IF), ID("a"), T(TOKEN_THEN), T(TOKEN_ELSE), INT("2"), NL);
}

/*
  STATEMENTS ACCEPTED
*/

static void statement_accepts_variable_definitions(void **state) {
	(void)state;
	// val x = 1
	MAIN_OK(T(TOKEN_VAL), ID("x"), T(TOKEN_ASSIGN), INT("1"), NL);
	// var y: Int = 1 + 2
	MAIN_OK(T(TOKEN_VAR), ID("y"), T(TOKEN_COLON), T(TOKEN_TYPE_INT), T(TOKEN_ASSIGN), INT("1"),
	        T(TOKEN_PLUS), INT("2"), NL);
	// each basic type
	MAIN_OK(T(TOKEN_VAL), ID("a"), T(TOKEN_COLON), T(TOKEN_TYPE_DOUBLE), T(TOKEN_ASSIGN), ID("d"), NL,
	        T(TOKEN_VAL), ID("b"), T(TOKEN_COLON), T(TOKEN_TYPE_STRING), T(TOKEN_ASSIGN), ID("s"), NL,
	        T(TOKEN_VAL), ID("c"), T(TOKEN_COLON), T(TOKEN_TYPE_BOOLEAN), T(TOKEN_ASSIGN), ID("t"), NL,
	        T(TOKEN_VAL), ID("u"), T(TOKEN_COLON), T(TOKEN_TYPE_UNIT), T(TOKEN_ASSIGN), T(TOKEN_LPAREN),
	        T(TOKEN_RPAREN), NL);
	// val m = if a then 1 else 2
	MAIN_OK(T(TOKEN_VAL), ID("m"), T(TOKEN_ASSIGN), T(TOKEN_IF), ID("a"), T(TOKEN_THEN), INT("1"),
	        T(TOKEN_ELSE), INT("2"), NL);
}

static void statement_accepts_assignment(void **state) {
	(void)state;
	// x = x + 1
	MAIN_OK(ID("x"), T(TOKEN_ASSIGN), ID("x"), T(TOKEN_PLUS), INT("1"), NL);
	// x = f(a, b).length
	MAIN_OK(ID("x"), T(TOKEN_ASSIGN), ID("f"), T(TOKEN_LPAREN), ID("a"), T(TOKEN_COMMA), ID("b"),
	        T(TOKEN_RPAREN), T(TOKEN_DOT), ID("length"), NL);
}

static void statement_accepts_expression_statements(void **state) {
	(void)state;
	// print(x)
	MAIN_OK(ID("print"), T(TOKEN_LPAREN), ID("x"), T(TOKEN_RPAREN), NL);
	// x + 1, statement starting with an identifier that isnt assigned
	MAIN_OK(ID("x"), T(TOKEN_PLUS), INT("1"), NL);
	// s.length
	MAIN_OK(ID("s"), T(TOKEN_DOT), ID("length"), NL);
	// 42
	MAIN_OK(INT("42"), NL);
	// (a + b) * c
	MAIN_OK(T(TOKEN_LPAREN), ID("a"), T(TOKEN_PLUS), ID("b"), T(TOKEN_RPAREN), T(TOKEN_MULTIPLY), ID("c"),
	        NL);
	// -x
	MAIN_OK(T(TOKEN_MINUS), ID("x"), NL);
	// ()
	MAIN_OK(T(TOKEN_LPAREN), T(TOKEN_RPAREN), NL);
}

static void statement_accepts_return(void **state) {
	(void)state;
	// return x + 1
	MAIN_OK(T(TOKEN_RETURN), ID("x"), T(TOKEN_PLUS), INT("1"), NL);
	// return
	MAIN_OK(T(TOKEN_RETURN), NL);
	// return as the last token of the block, no newline before dedent
	PROGRAM_OK(PROLOG, MAIN_HEAD, T(TOKEN_RETURN), DEDENT);
	// return if a then 1 else 2
	MAIN_OK(T(TOKEN_RETURN), T(TOKEN_IF), ID("a"), T(TOKEN_THEN), INT("1"), T(TOKEN_ELSE), INT("2"), NL);
}

static void statement_accepts_semicolons(void **state) {
	(void)state;
	// x = 1; y = 2
	MAIN_OK(ID("x"), T(TOKEN_ASSIGN), INT("1"), T(TOKEN_SEMICOLON), ID("y"), T(TOKEN_ASSIGN), INT("2"), NL);
	// x = 1;
	MAIN_OK(ID("x"), T(TOKEN_ASSIGN), INT("1"), T(TOKEN_SEMICOLON), NL);
}

static void statement_accepts_if_blocks(void **state) {
	(void)state;
	/* if a then
	     f()
	   else
	     g() */
	MAIN_OK(T(TOKEN_IF), ID("a"), T(TOKEN_THEN), NL, INDENT, ID("f"), T(TOKEN_LPAREN), T(TOKEN_RPAREN), NL,
	        DEDENT, T(TOKEN_ELSE), NL, INDENT, ID("g"), T(TOKEN_LPAREN), T(TOKEN_RPAREN), NL, DEDENT);
	/* if a then
	     f() */
	MAIN_OK(T(TOKEN_IF), ID("a"), T(TOKEN_THEN), NL, INDENT, ID("f"), T(TOKEN_LPAREN), T(TOKEN_RPAREN), NL,
	        DEDENT);
	/* if a then
	     x = 1
	   else if b then
	     x = 2
	   else
	     x = 3 */
	MAIN_OK(T(TOKEN_IF), ID("a"), T(TOKEN_THEN), NL, INDENT, ID("x"), T(TOKEN_ASSIGN), INT("1"), NL, DEDENT,
	        T(TOKEN_ELSE), T(TOKEN_IF), ID("b"), T(TOKEN_THEN), NL, INDENT, ID("x"), T(TOKEN_ASSIGN),
	        INT("2"), NL, DEDENT, T(TOKEN_ELSE), NL, INDENT, ID("x"), T(TOKEN_ASSIGN), INT("3"), NL, DEDENT);
}

static void statement_accepts_inline_if(void **state) {
	(void)state;
	// if a then x = 1 else x = 2
	MAIN_OK(T(TOKEN_IF), ID("a"), T(TOKEN_THEN), ID("x"), T(TOKEN_ASSIGN), INT("1"), T(TOKEN_ELSE), ID("x"),
	        T(TOKEN_ASSIGN), INT("2"), NL);
	/* if a then return 1
	   else return 2 */
	MAIN_OK(T(TOKEN_IF), ID("a"), T(TOKEN_THEN), T(TOKEN_RETURN), INT("1"), NL, T(TOKEN_ELSE),
	        T(TOKEN_RETURN), INT("2"), NL);
	// if a then return else f(1)
	MAIN_OK(T(TOKEN_IF), ID("a"), T(TOKEN_THEN), T(TOKEN_RETURN), T(TOKEN_ELSE), ID("f"), T(TOKEN_LPAREN),
	        INT("1"), T(TOKEN_RPAREN), NL);
	// if a then print(1)
	MAIN_OK(T(TOKEN_IF), ID("a"), T(TOKEN_THEN), ID("print"), T(TOKEN_LPAREN), INT("1"), T(TOKEN_RPAREN), NL);
	/* if a then x = 1 else
	     x = 2 */
	MAIN_OK(T(TOKEN_IF), ID("a"), T(TOKEN_THEN), ID("x"), T(TOKEN_ASSIGN), INT("1"), T(TOKEN_ELSE), NL,
	        INDENT, ID("x"), T(TOKEN_ASSIGN), INT("2"), NL, DEDENT);
}

static void statement_accepts_while(void **state) {
	(void)state;
	/* while i < n do
	     i = i + 1 */
	MAIN_OK(T(TOKEN_WHILE), ID("i"), T(TOKEN_LESS), ID("n"), T(TOKEN_DO), NL, INDENT, ID("i"),
	        T(TOKEN_ASSIGN), ID("i"), T(TOKEN_PLUS), INT("1"), NL, DEDENT);
	// while i < n do i = i + 1
	MAIN_OK(T(TOKEN_WHILE), ID("i"), T(TOKEN_LESS), ID("n"), T(TOKEN_DO), ID("i"), T(TOKEN_ASSIGN), ID("i"),
	        T(TOKEN_PLUS), INT("1"), NL);
}

static void statement_accepts_inline_for(void **state) {
	(void)state;
	// for i <- 1 to 10 do print(i)
	MAIN_OK(T(TOKEN_FOR), ID("i"), T(TOKEN_LEFT_ARROW), INT("1"), ID("to"), INT("10"), T(TOKEN_DO),
	        ID("print"), T(TOKEN_LPAREN), ID("i"), T(TOKEN_RPAREN), NL);
	/* for i <- 0 until n do
	     print(i) */
	MAIN_OK(T(TOKEN_FOR), ID("i"), T(TOKEN_LEFT_ARROW), INT("0"), ID("until"), ID("n"), T(TOKEN_DO), NL,
	        INDENT, ID("print"), T(TOKEN_LPAREN), ID("i"), T(TOKEN_RPAREN), NL, DEDENT);
	// for i <- 1 to 10 by 2 do print(i)
	MAIN_OK(T(TOKEN_FOR), ID("i"), T(TOKEN_LEFT_ARROW), INT("1"), ID("to"), INT("10"), ID("by"), INT("2"),
	        T(TOKEN_DO), ID("print"), T(TOKEN_LPAREN), ID("i"), T(TOKEN_RPAREN), NL);
	// for i <- a + 1 to f(b) by -1 do print(i)
	MAIN_OK(T(TOKEN_FOR), ID("i"), T(TOKEN_LEFT_ARROW), ID("a"), T(TOKEN_PLUS), INT("1"), ID("to"), ID("f"),
	        T(TOKEN_LPAREN), ID("b"), T(TOKEN_RPAREN), ID("by"), T(TOKEN_MINUS), INT("1"), T(TOKEN_DO),
	        ID("print"), T(TOKEN_LPAREN), ID("i"), T(TOKEN_RPAREN), NL);
	// for i <- 1 to 10 if i != 3 if i < 8 do print(i)
	MAIN_OK(T(TOKEN_FOR), ID("i"), T(TOKEN_LEFT_ARROW), INT("1"), ID("to"), INT("10"), T(TOKEN_IF), ID("i"),
	        T(TOKEN_NOT_EQUAL), INT("3"), T(TOKEN_IF), ID("i"), T(TOKEN_LESS), INT("8"), T(TOKEN_DO),
	        ID("print"), T(TOKEN_LPAREN), ID("i"), T(TOKEN_RPAREN), NL);
}

static void statement_accepts_multiline_for(void **state) {
	(void)state;
	/* for
	     i <- 0 until n
	     if i != 3
	     if i < 8
	   do
	     print(i) */
	MAIN_OK(T(TOKEN_FOR), NL, INDENT, ID("i"), T(TOKEN_LEFT_ARROW), INT("0"), ID("until"), ID("n"), NL,
	        T(TOKEN_IF), ID("i"), T(TOKEN_NOT_EQUAL), INT("3"), NL, T(TOKEN_IF), ID("i"), T(TOKEN_LESS),
	        INT("8"), NL, DEDENT, T(TOKEN_DO), NL, INDENT, ID("print"), T(TOKEN_LPAREN), ID("i"),
	        T(TOKEN_RPAREN), NL, DEDENT);
	/* for
	     i <- 1 to 3
	   do print(i) */
	MAIN_OK(T(TOKEN_FOR), NL, INDENT, ID("i"), T(TOKEN_LEFT_ARROW), INT("1"), ID("to"), INT("3"), NL, DEDENT,
	        T(TOKEN_DO), ID("print"), T(TOKEN_LPAREN), ID("i"), T(TOKEN_RPAREN), NL);
}

static void statement_accepts_nested_blocks(void **state) {
	(void)state;
	/* while a do
	     if b then
	       for i <- 1 to 2 do
	         x = i
	     else
	       return
	     y = 1 */
	MAIN_OK(T(TOKEN_WHILE), ID("a"), T(TOKEN_DO), NL, INDENT, T(TOKEN_IF), ID("b"), T(TOKEN_THEN), NL, INDENT,
	        T(TOKEN_FOR), ID("i"), T(TOKEN_LEFT_ARROW), INT("1"), ID("to"), INT("2"), T(TOKEN_DO), NL, INDENT,
	        ID("x"), T(TOKEN_ASSIGN), ID("i"), NL, DEDENT, DEDENT, T(TOKEN_ELSE), NL, INDENT, T(TOKEN_RETURN),
	        NL, DEDENT, ID("y"), T(TOKEN_ASSIGN), INT("1"), NL, DEDENT);
}

/*
  STATEMENTS REJECTED
*/

static void statement_rejects_bad_variable_definitions(void **state) {
	(void)state;
	// val = 1
	MAIN_REJECTED(T(TOKEN_VAL), T(TOKEN_ASSIGN), INT("1"), NL);
	// val x
	MAIN_REJECTED(T(TOKEN_VAL), ID("x"), NL);
	// val x =
	MAIN_REJECTED(T(TOKEN_VAL), ID("x"), T(TOKEN_ASSIGN), NL);
	// val x: = 1
	MAIN_REJECTED(T(TOKEN_VAL), ID("x"), T(TOKEN_COLON), T(TOKEN_ASSIGN), INT("1"), NL);
	// val x: Foo = 1
	MAIN_REJECTED(T(TOKEN_VAL), ID("x"), T(TOKEN_COLON), ID("Foo"), T(TOKEN_ASSIGN), INT("1"), NL);
	// var 1 = 2
	MAIN_REJECTED(T(TOKEN_VAR), INT("1"), T(TOKEN_ASSIGN), INT("2"), NL);
	// val x: Int 1
	MAIN_REJECTED(T(TOKEN_VAL), ID("x"), T(TOKEN_COLON), T(TOKEN_TYPE_INT), INT("1"), NL);
}

static void statement_rejects_bad_assignments(void **state) {
	(void)state;
	// x =
	MAIN_REJECTED(ID("x"), T(TOKEN_ASSIGN), NL);
	// 1 = 2
	MAIN_REJECTED(INT("1"), T(TOKEN_ASSIGN), INT("2"), NL);
	// x + 1 = 2
	MAIN_REJECTED(ID("x"), T(TOKEN_PLUS), INT("1"), T(TOKEN_ASSIGN), INT("2"), NL);
	// x = y = 1
	MAIN_REJECTED(ID("x"), T(TOKEN_ASSIGN), ID("y"), T(TOKEN_ASSIGN), INT("1"), NL);
}

static void statement_rejects_missing_separator(void **state) {
	(void)state;
	// x = 1 y = 2
	MAIN_REJECTED(ID("x"), T(TOKEN_ASSIGN), INT("1"), ID("y"), T(TOKEN_ASSIGN), INT("2"), NL);
	// x y
	MAIN_REJECTED(ID("x"), ID("y"), NL);
	// f() g()
	MAIN_REJECTED(ID("f"), T(TOKEN_LPAREN), T(TOKEN_RPAREN), ID("g"), T(TOKEN_LPAREN), T(TOKEN_RPAREN), NL);
}

static void statement_rejects_misplaced_keywords(void **state) {
	(void)state;
	// else alone
	MAIN_REJECTED(T(TOKEN_ELSE), ID("x"), NL);
	// then alone
	MAIN_REJECTED(T(TOKEN_THEN), ID("x"), NL);
	// do alone
	MAIN_REJECTED(T(TOKEN_DO), ID("x"), NL);
	// def inside a function body
	MAIN_REJECTED(T(TOKEN_DEF), ID("g"), T(TOKEN_LPAREN), T(TOKEN_RPAREN), T(TOKEN_ASSIGN), INT("1"), NL);
	// import inside a function body
	MAIN_REJECTED(PROLOG);
}

static void statement_rejects_bad_if(void **state) {
	(void)state;
	/* if a
	     f() */
	MAIN_REJECTED(T(TOKEN_IF), ID("a"), NL, INDENT, ID("f"), T(TOKEN_LPAREN), T(TOKEN_RPAREN), NL, DEDENT);
	// if then x = 1
	MAIN_REJECTED(T(TOKEN_IF), T(TOKEN_THEN), ID("x"), T(TOKEN_ASSIGN), INT("1"), NL);
	// if a then
	MAIN_REJECTED(T(TOKEN_IF), ID("a"), T(TOKEN_THEN), NL);
	// if a then x = 1 else
	MAIN_REJECTED(T(TOKEN_IF), ID("a"), T(TOKEN_THEN), ID("x"), T(TOKEN_ASSIGN), INT("1"), T(TOKEN_ELSE), NL);
	/* if a then
	     f()
	   else else */
	MAIN_REJECTED(T(TOKEN_IF), ID("a"), T(TOKEN_THEN), NL, INDENT, ID("f"), T(TOKEN_LPAREN), T(TOKEN_RPAREN),
	              NL, DEDENT, T(TOKEN_ELSE), T(TOKEN_ELSE), NL);
}

static void statement_rejects_bad_while(void **state) {
	(void)state;
	/* while a
	     f() */
	MAIN_REJECTED(T(TOKEN_WHILE), ID("a"), NL, INDENT, ID("f"), T(TOKEN_LPAREN), T(TOKEN_RPAREN), NL, DEDENT);
	// while do f()
	MAIN_REJECTED(T(TOKEN_WHILE), T(TOKEN_DO), ID("f"), T(TOKEN_LPAREN), T(TOKEN_RPAREN), NL);
	// while a do
	MAIN_REJECTED(T(TOKEN_WHILE), ID("a"), T(TOKEN_DO), NL);
}

static void statement_rejects_bad_for(void **state) {
	(void)state;
	// for i = 1 to 3 do f()
	MAIN_REJECTED(T(TOKEN_FOR), ID("i"), T(TOKEN_ASSIGN), INT("1"), ID("to"), INT("3"), T(TOKEN_DO), ID("f"),
	              T(TOKEN_LPAREN), T(TOKEN_RPAREN), NL);
	// for 1 <- 1 to 3 do f()
	MAIN_REJECTED(T(TOKEN_FOR), INT("1"), T(TOKEN_LEFT_ARROW), INT("1"), ID("to"), INT("3"), T(TOKEN_DO),
	              ID("f"), T(TOKEN_LPAREN), T(TOKEN_RPAREN), NL);
	// for i <- 1 to do f()
	MAIN_REJECTED(T(TOKEN_FOR), ID("i"), T(TOKEN_LEFT_ARROW), INT("1"), ID("to"), T(TOKEN_DO), ID("f"),
	              T(TOKEN_LPAREN), T(TOKEN_RPAREN), NL);
	// for i <- 1 by 2 do f(), step without a range
	MAIN_REJECTED(T(TOKEN_FOR), ID("i"), T(TOKEN_LEFT_ARROW), INT("1"), ID("by"), INT("2"), T(TOKEN_DO),
	              ID("f"), T(TOKEN_LPAREN), T(TOKEN_RPAREN), NL);
	// for i <- 1 to 3 by do f()
	MAIN_REJECTED(T(TOKEN_FOR), ID("i"), T(TOKEN_LEFT_ARROW), INT("1"), ID("to"), INT("3"), ID("by"),
	              T(TOKEN_DO), ID("f"), T(TOKEN_LPAREN), T(TOKEN_RPAREN), NL);
	// for i <- 1 to 3 f()
	MAIN_REJECTED(T(TOKEN_FOR), ID("i"), T(TOKEN_LEFT_ARROW), INT("1"), ID("to"), INT("3"), ID("f"),
	              T(TOKEN_LPAREN), T(TOKEN_RPAREN), NL);
	// for i <- 1 to 3 if do f()
	MAIN_REJECTED(T(TOKEN_FOR), ID("i"), T(TOKEN_LEFT_ARROW), INT("1"), ID("to"), INT("3"), T(TOKEN_IF),
	              T(TOKEN_DO), ID("f"), T(TOKEN_LPAREN), T(TOKEN_RPAREN), NL);
}

static void statement_rejects_bad_multiline_for(void **state) {
	(void)state;
	/* for
	     i <- 1 to 3 if i > 1
	   do f()
	   in the multi line form every guard needs its own line */
	MAIN_REJECTED(T(TOKEN_FOR), NL, INDENT, ID("i"), T(TOKEN_LEFT_ARROW), INT("1"), ID("to"), INT("3"),
	              T(TOKEN_IF), ID("i"), T(TOKEN_GREATER), INT("1"), NL, DEDENT, T(TOKEN_DO), ID("f"),
	              T(TOKEN_LPAREN), T(TOKEN_RPAREN), NL);
	/* for
	   do f() */
	MAIN_REJECTED(T(TOKEN_FOR), NL, T(TOKEN_DO), ID("f"), T(TOKEN_LPAREN), T(TOKEN_RPAREN), NL);
	/* for
	     i <- 1 to 3
	     do f()
	   do has to go back to the indentation of for */
	MAIN_REJECTED(T(TOKEN_FOR), NL, INDENT, ID("i"), T(TOKEN_LEFT_ARROW), INT("1"), ID("to"), INT("3"), NL,
	              T(TOKEN_DO), ID("f"), T(TOKEN_LPAREN), T(TOKEN_RPAREN), NL, DEDENT);
}

static void statement_rejects_empty_block(void **state) {
	(void)state;
	// main body with no statement
	PROGRAM_REJECTED(PROLOG, MAIN_HEAD, DEDENT);
	// if a then followed by an empty block
	MAIN_REJECTED(T(TOKEN_IF), ID("a"), T(TOKEN_THEN), NL, INDENT, DEDENT);
}

static void statement_rejects_unclosed_block(void **state) {
	(void)state;
	// the main body never gets a dedent
	PROGRAM_REJECTED(PROLOG, MAIN_HEAD, ID("x"), T(TOKEN_ASSIGN), INT("1"), NL);
}

/*
  PROGRAMS
*/

static void program_accepts_minimal_main(void **state) {
	(void)state;
	/* import ifj26.*
	   @main def main(): Unit =
	     print("hi") */
	MAIN_OK(ID("print"), T(TOKEN_LPAREN), TX(TOKEN_STRING, "hi"), T(TOKEN_RPAREN), NL);
}

static void program_accepts_prolog_only(void **state) {
	(void)state;
	// missing main is a semantic error, syntax is fine
	PROGRAM_OK(PROLOG);
	// import ifj26.* with no newline before end of file
	PROGRAM_OK(T(TOKEN_IMPORT), ID("ifj26"), T(TOKEN_DOT), T(TOKEN_MULTIPLY));
}

static void program_accepts_main_annotation_on_own_line(void **state) {
	(void)state;
	/* @main
	   def main() =
	     return */
	PROGRAM_OK(PROLOG, T(TOKEN_MAIN_ANNOTATION), NL, T(TOKEN_DEF), ID("main"), T(TOKEN_LPAREN),
	           T(TOKEN_RPAREN), T(TOKEN_ASSIGN), NL, INDENT, T(TOKEN_RETURN), NL, DEDENT);
}

static void program_accepts_function_signatures(void **state) {
	(void)state;
	/* def f(a: Int, b: String): Double =
	     return 1.5 */
	PROGRAM_OK(PROLOG, T(TOKEN_DEF), ID("f"), T(TOKEN_LPAREN), ID("a"), T(TOKEN_COLON), T(TOKEN_TYPE_INT),
	           T(TOKEN_COMMA), ID("b"), T(TOKEN_COLON), T(TOKEN_TYPE_STRING), T(TOKEN_RPAREN), T(TOKEN_COLON),
	           T(TOKEN_TYPE_DOUBLE), T(TOKEN_ASSIGN), NL, INDENT, T(TOKEN_RETURN), TX(TOKEN_DOUBLE, "1.5"),
	           NL, DEDENT);
	/* def g() =
	     h() */
	PROGRAM_OK(PROLOG, T(TOKEN_DEF), ID("g"), T(TOKEN_LPAREN), T(TOKEN_RPAREN), T(TOKEN_ASSIGN), NL, INDENT,
	           ID("h"), T(TOKEN_LPAREN), T(TOKEN_RPAREN), NL, DEDENT);
	/* def f(
	     a: Int,
	     b: Boolean,
	   ): Unit = () */
	PROGRAM_OK(PROLOG, T(TOKEN_DEF), ID("f"), T(TOKEN_LPAREN), ID("a"), T(TOKEN_COLON), T(TOKEN_TYPE_INT),
	           T(TOKEN_COMMA), ID("b"), T(TOKEN_COLON), T(TOKEN_TYPE_BOOLEAN), T_AT(TOKEN_COMMA, 3),
	           T_AT(TOKEN_RPAREN, 4), T(TOKEN_COLON), T(TOKEN_TYPE_UNIT), T(TOKEN_ASSIGN), T(TOKEN_LPAREN),
	           T(TOKEN_RPAREN), NL);
}

static void program_accepts_inline_function_bodies(void **state) {
	(void)state;
	// def inc(a: Int): Int = a + 1
	PROGRAM_OK(PROLOG, T(TOKEN_DEF), ID("inc"), T(TOKEN_LPAREN), ID("a"), T(TOKEN_COLON), T(TOKEN_TYPE_INT),
	           T(TOKEN_RPAREN), T(TOKEN_COLON), T(TOKEN_TYPE_INT), T(TOKEN_ASSIGN), ID("a"), T(TOKEN_PLUS),
	           INT("1"), NL);
	// def abs(x: Int): Int = if x < 0 then -x else x
	PROGRAM_OK(PROLOG, T(TOKEN_DEF), ID("abs"), T(TOKEN_LPAREN), ID("x"), T(TOKEN_COLON), T(TOKEN_TYPE_INT),
	           T(TOKEN_RPAREN), T(TOKEN_COLON), T(TOKEN_TYPE_INT), T(TOKEN_ASSIGN), T(TOKEN_IF), ID("x"),
	           T(TOKEN_LESS), INT("0"), T(TOKEN_THEN), T(TOKEN_MINUS), ID("x"), T(TOKEN_ELSE), ID("x"), NL);
	// def f(): Int = 1 at the very end of the file without a newline
	PROGRAM_OK(PROLOG, T(TOKEN_DEF), ID("f"), T(TOKEN_LPAREN), T(TOKEN_RPAREN), T(TOKEN_COLON),
	           T(TOKEN_TYPE_INT), T(TOKEN_ASSIGN), INT("1"));
}

static void program_accepts_several_functions(void **state) {
	(void)state;
	/* def factorial(n: Int): Int =
	     if n < 2 then
	       return 1
	     else
	       return n * factorial(n - 1)

	   @main def main(): Unit =
	     var i = 0
	     while i < 5 do
	       print(factorial(i))
	       i = i + 1 */
	PROGRAM_OK(PROLOG, T(TOKEN_DEF), ID("factorial"), T(TOKEN_LPAREN), ID("n"), T(TOKEN_COLON),
	           T(TOKEN_TYPE_INT), T(TOKEN_RPAREN), T(TOKEN_COLON), T(TOKEN_TYPE_INT), T(TOKEN_ASSIGN), NL,
	           INDENT, T(TOKEN_IF), ID("n"), T(TOKEN_LESS), INT("2"), T(TOKEN_THEN), NL, INDENT,
	           T(TOKEN_RETURN), INT("1"), NL, DEDENT, T(TOKEN_ELSE), NL, INDENT, T(TOKEN_RETURN), ID("n"),
	           T(TOKEN_MULTIPLY), ID("factorial"), T(TOKEN_LPAREN), ID("n"), T(TOKEN_MINUS), INT("1"),
	           T(TOKEN_RPAREN), NL, DEDENT, DEDENT, MAIN_HEAD, T(TOKEN_VAR), ID("i"), T(TOKEN_ASSIGN),
	           INT("0"), NL, T(TOKEN_WHILE), ID("i"), T(TOKEN_LESS), INT("5"), T(TOKEN_DO), NL, INDENT,
	           ID("print"), T(TOKEN_LPAREN), ID("factorial"), T(TOKEN_LPAREN), ID("i"), T(TOKEN_RPAREN),
	           T(TOKEN_RPAREN), NL, ID("i"), T(TOKEN_ASSIGN), ID("i"), T(TOKEN_PLUS), INT("1"), NL, DEDENT,
	           DEDENT);
}

static void program_rejects_bad_prolog(void **state) {
	(void)state;
	// no prolog
	PROGRAM_REJECTED(MAIN_HEAD, T(TOKEN_RETURN), NL, DEDENT);
	// import other.*
	PROGRAM_REJECTED(T(TOKEN_IMPORT), ID("other"), T(TOKEN_DOT), T(TOKEN_MULTIPLY), NL);
	// import ifj2.*, same length check is not enough
	PROGRAM_REJECTED(T(TOKEN_IMPORT), ID("ifj2"), T(TOKEN_DOT), T(TOKEN_MULTIPLY), NL);
	// import ifj26x.*
	PROGRAM_REJECTED(T(TOKEN_IMPORT), ID("ifj26x"), T(TOKEN_DOT), T(TOKEN_MULTIPLY), NL);
	// import ifj26
	PROGRAM_REJECTED(T(TOKEN_IMPORT), ID("ifj26"), NL);
	// import ifj26.print
	PROGRAM_REJECTED(T(TOKEN_IMPORT), ID("ifj26"), T(TOKEN_DOT), ID("print"), NL);
	// import ifj26.* def f() = 1, prolog has to end the line
	PROGRAM_REJECTED(T(TOKEN_IMPORT), ID("ifj26"), T(TOKEN_DOT), T(TOKEN_MULTIPLY), T(TOKEN_DEF), ID("f"),
	                 T(TOKEN_LPAREN), T(TOKEN_RPAREN), T(TOKEN_ASSIGN), INT("1"), NL);
	// prolog written twice
	PROGRAM_REJECTED(PROLOG, PROLOG);
}

static void program_rejects_top_level_statements(void **state) {
	(void)state;
	// val x = 1 outside of a function
	PROGRAM_REJECTED(PROLOG, T(TOKEN_VAL), ID("x"), T(TOKEN_ASSIGN), INT("1"), NL);
	// print(1) outside of a function
	PROGRAM_REJECTED(PROLOG, ID("print"), T(TOKEN_LPAREN), INT("1"), T(TOKEN_RPAREN), NL);
	// stray indented block at top level
	PROGRAM_REJECTED(PROLOG, INDENT, ID("x"), NL, DEDENT);
}

static void program_rejects_bad_function_headers(void **state) {
	(void)state;
	// def () = 1
	PROGRAM_REJECTED(PROLOG, T(TOKEN_DEF), T(TOKEN_LPAREN), T(TOKEN_RPAREN), T(TOKEN_ASSIGN), INT("1"), NL);
	// def f = 1
	PROGRAM_REJECTED(PROLOG, T(TOKEN_DEF), ID("f"), T(TOKEN_ASSIGN), INT("1"), NL);
	// def f() 1
	PROGRAM_REJECTED(PROLOG, T(TOKEN_DEF), ID("f"), T(TOKEN_LPAREN), T(TOKEN_RPAREN), INT("1"), NL);
	// def f(): = 1
	PROGRAM_REJECTED(PROLOG, T(TOKEN_DEF), ID("f"), T(TOKEN_LPAREN), T(TOKEN_RPAREN), T(TOKEN_COLON),
	                 T(TOKEN_ASSIGN), INT("1"), NL);
	// def f(a) = 1, parameters need a type
	PROGRAM_REJECTED(PROLOG, T(TOKEN_DEF), ID("f"), T(TOKEN_LPAREN), ID("a"), T(TOKEN_RPAREN),
	                 T(TOKEN_ASSIGN), INT("1"), NL);
	// def f(a: Int,) = 1 on one line
	PROGRAM_REJECTED(PROLOG, T(TOKEN_DEF), ID("f"), T(TOKEN_LPAREN), ID("a"), T(TOKEN_COLON),
	                 T(TOKEN_TYPE_INT), T(TOKEN_COMMA), T(TOKEN_RPAREN), T(TOKEN_ASSIGN), INT("1"), NL);
	// def f(a: Int b: Int) = 1
	PROGRAM_REJECTED(PROLOG, T(TOKEN_DEF), ID("f"), T(TOKEN_LPAREN), ID("a"), T(TOKEN_COLON),
	                 T(TOKEN_TYPE_INT), ID("b"), T(TOKEN_COLON), T(TOKEN_TYPE_INT), T(TOKEN_RPAREN),
	                 T(TOKEN_ASSIGN), INT("1"), NL);
	// def f(, a: Int) = 1
	PROGRAM_REJECTED(PROLOG, T(TOKEN_DEF), ID("f"), T(TOKEN_LPAREN), T(TOKEN_COMMA), ID("a"), T(TOKEN_COLON),
	                 T(TOKEN_TYPE_INT), T(TOKEN_RPAREN), T(TOKEN_ASSIGN), INT("1"), NL);
	/* def f() =
	   with nothing after it */
	PROGRAM_REJECTED(PROLOG, T(TOKEN_DEF), ID("f"), T(TOKEN_LPAREN), T(TOKEN_RPAREN), T(TOKEN_ASSIGN), NL);
	// @main @main def f() = 1
	PROGRAM_REJECTED(PROLOG, T(TOKEN_MAIN_ANNOTATION), T(TOKEN_MAIN_ANNOTATION), T(TOKEN_DEF), ID("f"),
	                 T(TOKEN_LPAREN), T(TOKEN_RPAREN), T(TOKEN_ASSIGN), INT("1"), NL);
	// @main alone at the end
	PROGRAM_REJECTED(PROLOG, T(TOKEN_MAIN_ANNOTATION), NL);
}

static void program_rejects_tokens_after_function(void **state) {
	(void)state;
	// def f(): Int = 1 2
	PROGRAM_REJECTED(PROLOG, T(TOKEN_DEF), ID("f"), T(TOKEN_LPAREN), T(TOKEN_RPAREN), T(TOKEN_COLON),
	                 T(TOKEN_TYPE_INT), T(TOKEN_ASSIGN), INT("1"), INT("2"), NL);
	// extra dedent after main
	PROGRAM_REJECTED(PROLOG, MAIN_HEAD, T(TOKEN_RETURN), NL, DEDENT, DEDENT);
}

/*
  LEXER AND MEMORY ERRORS
*/

static void lexer_error_stops_expression(void **state) {
	(void)state;
	// a + b where the lexer fails while reading the tokens after a
	LOAD(ID("a"), T(TOKEN_PLUS), ID("b"), NL);
	fake_failing_advance = 2;
	assert_syntax_error(syntax_parse_expression(&parser));
	assert_int_equal(fake_advance_calls, 2);
}

static void lexer_error_stops_program(void **state) {
	(void)state;
	/* x = 1 in the main body, lexer fails while reading the token after =
	   (the 17th advance, the prolog and main header take the first 15) */
	LOAD_MAIN(ID("x"), T(TOKEN_ASSIGN), INT("1"), NL);
	fake_failing_advance = 17;
	assert_program_rejected();
	assert_int_equal(fake_advance_calls, 17);
}

static void out_of_memory_is_internal_error(void **state) {
	(void)state;
	LOAD(ID("a"), T(TOKEN_PLUS), ID("b"), NL);
	fake_realloc_budget = 0;
	assert_false(syntax_parse_expression(&parser));
	assert_true(parser.failed);
	assert_true(parser.internal_error);
}

static void out_of_memory_while_stack_grows(void **state) {
	(void)state;
	// 20 open parentheses need more than the first 16 stack items
	LOAD(T(TOKEN_LPAREN), T(TOKEN_LPAREN), T(TOKEN_LPAREN), T(TOKEN_LPAREN), T(TOKEN_LPAREN), T(TOKEN_LPAREN),
	     T(TOKEN_LPAREN), T(TOKEN_LPAREN), T(TOKEN_LPAREN), T(TOKEN_LPAREN), T(TOKEN_LPAREN), T(TOKEN_LPAREN),
	     T(TOKEN_LPAREN), T(TOKEN_LPAREN), T(TOKEN_LPAREN), T(TOKEN_LPAREN), T(TOKEN_LPAREN), T(TOKEN_LPAREN),
	     T(TOKEN_LPAREN), T(TOKEN_LPAREN), ID("x"), NL);
	fake_realloc_budget = 1;
	assert_false(syntax_parse_expression(&parser));
	assert_true(parser.failed);
	assert_true(parser.internal_error);
}

static void out_of_memory_inside_program(void **state) {
	(void)state;
	LOAD_MAIN(ID("x"), T(TOKEN_ASSIGN), INT("1"), NL);
	fake_realloc_budget = 0;
	assert_false(syntax_parse_program(&parser));
	assert_true(parser.internal_error);
}

/*
  MAIN
*/

int main(void) {
	int test_results = 0;

	const struct CMUnitTest api_tests[] = {
	    cmocka_unit_test(init_rejects_null_arguments),
	    cmocka_unit_test(init_rejects_lexer_without_tokens),
	    cmocka_unit_test(init_resets_parser_state),
	    cmocka_unit_test(current_and_next_show_lookahead),
	    cmocka_unit_test(current_and_next_handle_null_parser),
	    cmocka_unit_test(expect_consumes_matching_token),
	    cmocka_unit_test(expect_mismatch_fails_parser),
	    cmocka_unit_test(failed_parser_hides_tokens_and_stops),
	    cmocka_unit_test(accept_consumes_only_matching_token),
	    cmocka_unit_test(accept_rejects_missing_result_pointer),
	    cmocka_unit_test(advance_reports_lexer_error),
	};

	const struct CMUnitTest expression_ok_tests[] = {
	    cmocka_unit_test(expression_accepts_single_operands),
	    cmocka_unit_test(expression_accepts_arithmetic),
	    cmocka_unit_test(expression_accepts_parentheses),
	    cmocka_unit_test(expression_accepts_unary_operators),
	    cmocka_unit_test(expression_accepts_relational_and_logical),
	    cmocka_unit_test(expression_accepts_calls),
	    cmocka_unit_test(expression_accepts_trailing_comma_before_line_break),
	    cmocka_unit_test(expression_accepts_members),
	    cmocka_unit_test(expression_accepts_if_expression),
	    cmocka_unit_test(expression_accepts_deep_nesting),
	    cmocka_unit_test(expression_accepts_long_operator_chain),
	};

	const struct CMUnitTest expression_stop_tests[] = {
	    cmocka_unit_test(expression_leaves_terminators_for_caller),
	    cmocka_unit_test(expression_leaves_keywords_for_caller),
	    cmocka_unit_test(expression_leaves_delimiters_for_caller),
	    cmocka_unit_test(expression_stops_at_operand_after_operand),
	};

	const struct CMUnitTest expression_error_tests[] = {
	    cmocka_unit_test(expression_rejects_empty_input),
	    cmocka_unit_test(expression_rejects_missing_operands),
	    cmocka_unit_test(expression_rejects_chained_relations),
	    cmocka_unit_test(expression_rejects_unbalanced_parentheses),
	    cmocka_unit_test(expression_rejects_bad_calls),
	    cmocka_unit_test(expression_rejects_bad_members),
	    cmocka_unit_test(expression_rejects_incomplete_if_expression),
	};

	const struct CMUnitTest statement_ok_tests[] = {
	    cmocka_unit_test(statement_accepts_variable_definitions),
	    cmocka_unit_test(statement_accepts_assignment),
	    cmocka_unit_test(statement_accepts_expression_statements),
	    cmocka_unit_test(statement_accepts_return),
	    cmocka_unit_test(statement_accepts_semicolons),
	    cmocka_unit_test(statement_accepts_if_blocks),
	    cmocka_unit_test(statement_accepts_inline_if),
	    cmocka_unit_test(statement_accepts_while),
	    cmocka_unit_test(statement_accepts_inline_for),
	    cmocka_unit_test(statement_accepts_multiline_for),
	    cmocka_unit_test(statement_accepts_nested_blocks),
	};

	const struct CMUnitTest statement_error_tests[] = {
	    cmocka_unit_test(statement_rejects_bad_variable_definitions),
	    cmocka_unit_test(statement_rejects_bad_assignments),
	    cmocka_unit_test(statement_rejects_missing_separator),
	    cmocka_unit_test(statement_rejects_misplaced_keywords),
	    cmocka_unit_test(statement_rejects_bad_if),
	    cmocka_unit_test(statement_rejects_bad_while),
	    cmocka_unit_test(statement_rejects_bad_for),
	    cmocka_unit_test(statement_rejects_bad_multiline_for),
	    cmocka_unit_test(statement_rejects_empty_block),
	    cmocka_unit_test(statement_rejects_unclosed_block),
	};

	const struct CMUnitTest program_tests[] = {
	    cmocka_unit_test(program_accepts_minimal_main),
	    cmocka_unit_test(program_accepts_prolog_only),
	    cmocka_unit_test(program_accepts_main_annotation_on_own_line),
	    cmocka_unit_test(program_accepts_function_signatures),
	    cmocka_unit_test(program_accepts_inline_function_bodies),
	    cmocka_unit_test(program_accepts_several_functions),
	    cmocka_unit_test(program_rejects_bad_prolog),
	    cmocka_unit_test(program_rejects_top_level_statements),
	    cmocka_unit_test(program_rejects_bad_function_headers),
	    cmocka_unit_test(program_rejects_tokens_after_function),
	};

	const struct CMUnitTest failure_tests[] = {
	    cmocka_unit_test(lexer_error_stops_expression),    cmocka_unit_test(lexer_error_stops_program),
	    cmocka_unit_test(out_of_memory_is_internal_error), cmocka_unit_test(out_of_memory_while_stack_grows),
	    cmocka_unit_test(out_of_memory_inside_program),
	};

	EXEC_TEST_CATEGORY("PARSER API", api_tests);
	EXEC_TEST_CATEGORY("EXPRESSIONS ACCEPTED", expression_ok_tests);
	EXEC_TEST_CATEGORY("WHERE EXPRESSIONS STOP", expression_stop_tests);
	EXEC_TEST_CATEGORY("EXPRESSIONS REJECTED", expression_error_tests);
	EXEC_TEST_CATEGORY("STATEMENTS ACCEPTED", statement_ok_tests);
	EXEC_TEST_CATEGORY("STATEMENTS REJECTED", statement_error_tests);
	EXEC_TEST_CATEGORY("PROGRAMS", program_tests);
	EXEC_TEST_CATEGORY("LEXER AND MEMORY ERRORS", failure_tests);

	return test_results;
}
