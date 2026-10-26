#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>

#include <cmocka.h>

#include "syntax_anal.h"

/* Link this test with src/syntax_anal.c and CMocka; it supplies the lexer API itself. */

#define MAX_RECORDED_ACTIONS 64

static const token_t *mock_tokens;
static size_t mock_token_count;
static size_t mock_token_index;
static syntax_semantic_action_t recorded_actions[MAX_RECORDED_ACTIONS];
static token_type_t recorded_action_token_types[MAX_RECORDED_ACTIONS];
static size_t recorded_action_count;
static size_t last_call_argument_count;
static lexer_t mock_lexer;

#define TOKEN_WITH_TEXT(token_kind, text)                                                                          \
	{                                                                                                            \
	    .type = (token_kind), .location = {1, 1}, .lexeme = (char *)(text), .length = sizeof(text) - 1          \
	}
#define TOKEN_ONLY(token_kind) {.type = (token_kind), .location = {1, 1}, .lexeme = NULL, .length = 0}

/* Mock lexer API. These tests feed tokens directly and never scan source text. */
const token_t *lexer_current(const lexer_t *lexer) {
	(void)lexer;
	if (mock_tokens == NULL || mock_token_count == 0 || mock_token_index >= mock_token_count) {
		return NULL;
	}
	return &mock_tokens[mock_token_index];
}

const token_t *lexer_next(const lexer_t *lexer) {
	(void)lexer;
	if (mock_tokens == NULL || mock_token_count == 0) {
		return NULL;
	}
	size_t next_index = mock_token_index + 1;
	if (next_index >= mock_token_count) {
		next_index = mock_token_count - 1;
	}
	return &mock_tokens[next_index];
}

lexer_result_t lexer_advance(lexer_t *lexer) {
	(void)lexer;
	if (mock_tokens == NULL || mock_token_count == 0) {
		return LEXER_RESULT_INVALID_ARGUMENT;
	}
	if (mock_token_index + 1 < mock_token_count) {
		mock_token_index++;
	}
	return LEXER_RESULT_OK;
}

/* Mock semantic-action sink; tests inspect which actions the parser emitted. */
void syntax_semantic_action(syntax_semantic_action_t action, const token_t *token, size_t item_count) {
	if (recorded_action_count < MAX_RECORDED_ACTIONS) {
		recorded_actions[recorded_action_count++] = action;
		recorded_action_token_types[recorded_action_count - 1] = token == NULL ? TOKEN_EOF : token->type;
	}
	if (action == SEMANTIC_ACTION_CALL) {
		last_call_argument_count = item_count;
	}
}

static void reset_mocks(const token_t *tokens, size_t count) {
	mock_tokens = tokens;
	mock_token_count = count;
	mock_token_index = 0;
	mock_lexer = (lexer_t){0};
	recorded_action_count = 0;
	last_call_argument_count = 0;
}

static bool init_parser(syntax_parser_t *parser, const token_t *tokens, size_t count) {
	reset_mocks(tokens, count);
	return syntax_parser_init(parser, &mock_lexer);
}

static size_t count_action(syntax_semantic_action_t action) {
	size_t count = 0;
	for (size_t i = 0; i < recorded_action_count; i++) {
		if (recorded_actions[i] == action) {
			count++;
		}
	}
	return count;
}

static void parses_precedence_and_reduces_inner_operator_first(void **state) {
	const token_t tokens[] = {
	    TOKEN_WITH_TEXT(TOKEN_INTEGER, "1"), TOKEN_ONLY(TOKEN_PLUS), TOKEN_WITH_TEXT(TOKEN_INTEGER, "2"),
	    TOKEN_ONLY(TOKEN_MULTIPLY), TOKEN_WITH_TEXT(TOKEN_INTEGER, "3"), TOKEN_ONLY(TOKEN_NEWLINE),
	    TOKEN_ONLY(TOKEN_EOF),
	};
	syntax_parser_t parser;
	assert_true(init_parser(&parser, tokens, sizeof(tokens) / sizeof(tokens[0])));
	assert_true(syntax_parse_expression(&parser));
	assert_int_equal(syntax_parser_current(&parser)->type, TOKEN_NEWLINE);
	assert_int_equal(count_action(SEMANTIC_ACTION_PRIMARY), 3);
	assert_int_equal(count_action(SEMANTIC_ACTION_BINARY), 2);
	assert_int_equal(recorded_actions[3], SEMANTIC_ACTION_BINARY);
	assert_int_equal(recorded_actions[4], SEMANTIC_ACTION_BINARY);
	assert_int_equal(recorded_action_token_types[3], TOKEN_MULTIPLY);
	assert_int_equal(recorded_action_token_types[4], TOKEN_PLUS);
	(void)state;
}

static void rejects_chained_relational_operators(void **state) {
	const token_t tokens[] = {
	    TOKEN_WITH_TEXT(TOKEN_INTEGER, "1"), TOKEN_ONLY(TOKEN_LESS), TOKEN_WITH_TEXT(TOKEN_INTEGER, "2"),
	    TOKEN_ONLY(TOKEN_LESS), TOKEN_WITH_TEXT(TOKEN_INTEGER, "3"), TOKEN_ONLY(TOKEN_NEWLINE),
	    TOKEN_ONLY(TOKEN_EOF),
	};
	syntax_parser_t parser;
	assert_true(init_parser(&parser, tokens, sizeof(tokens) / sizeof(tokens[0])));
	assert_false(syntax_parse_expression(&parser));
	assert_true(parser.failed);
	assert_int_equal(count_action(SEMANTIC_ACTION_BINARY), 1);
	(void)state;
}

static void recognizes_unit_and_nested_call_suffixes(void **state) {
	const token_t unit_tokens[] = {TOKEN_ONLY(TOKEN_LPAREN), TOKEN_ONLY(TOKEN_RPAREN), TOKEN_ONLY(TOKEN_NEWLINE),
	                               TOKEN_ONLY(TOKEN_EOF)};
	syntax_parser_t parser;
	assert_true(init_parser(&parser, unit_tokens, sizeof(unit_tokens) / sizeof(unit_tokens[0])));
	assert_true(syntax_parse_expression(&parser));
	assert_int_equal(count_action(SEMANTIC_ACTION_UNIT), 1);

	const token_t call_tokens[] = {
	    TOKEN_WITH_TEXT(TOKEN_IDENTIFIER, "f"), TOKEN_ONLY(TOKEN_LPAREN),
	    TOKEN_WITH_TEXT(TOKEN_IDENTIFIER, "g"), TOKEN_ONLY(TOKEN_LPAREN), TOKEN_WITH_TEXT(TOKEN_INTEGER, "1"),
	    TOKEN_ONLY(TOKEN_RPAREN), TOKEN_ONLY(TOKEN_COMMA), TOKEN_WITH_TEXT(TOKEN_INTEGER, "2"),
	    TOKEN_ONLY(TOKEN_RPAREN), TOKEN_ONLY(TOKEN_NEWLINE), TOKEN_ONLY(TOKEN_EOF),
	};
	assert_true(init_parser(&parser, call_tokens, sizeof(call_tokens) / sizeof(call_tokens[0])));
	assert_true(syntax_parse_expression(&parser));
	assert_int_equal(count_action(SEMANTIC_ACTION_CALL), 2);
	assert_int_equal(last_call_argument_count, 2);
	(void)state;
}

static void rejects_missing_right_operand(void **state) {
	const token_t tokens[] = {TOKEN_WITH_TEXT(TOKEN_INTEGER, "1"), TOKEN_ONLY(TOKEN_PLUS),
	                         TOKEN_ONLY(TOKEN_NEWLINE), TOKEN_ONLY(TOKEN_EOF)};
	syntax_parser_t parser;
	assert_true(init_parser(&parser, tokens, sizeof(tokens) / sizeof(tokens[0])));
	assert_false(syntax_parse_expression(&parser));
	assert_true(parser.failed);
	(void)state;
}

static void accepts_complete_program_from_mock_tokens(void **state) {
	const token_t tokens[] = {
	    TOKEN_ONLY(TOKEN_IMPORT), TOKEN_WITH_TEXT(TOKEN_IDENTIFIER, "ifj26"), TOKEN_ONLY(TOKEN_DOT),
	    TOKEN_ONLY(TOKEN_MULTIPLY), TOKEN_ONLY(TOKEN_NEWLINE), TOKEN_ONLY(TOKEN_MAIN_ANNOTATION),
	    TOKEN_ONLY(TOKEN_NEWLINE), TOKEN_ONLY(TOKEN_DEF), TOKEN_WITH_TEXT(TOKEN_IDENTIFIER, "main"),
	    TOKEN_ONLY(TOKEN_LPAREN), TOKEN_ONLY(TOKEN_RPAREN), TOKEN_ONLY(TOKEN_ASSIGN), TOKEN_ONLY(TOKEN_NEWLINE),
	    TOKEN_ONLY(TOKEN_INDENT), TOKEN_ONLY(TOKEN_RETURN), TOKEN_ONLY(TOKEN_NEWLINE), TOKEN_ONLY(TOKEN_DEDENT),
	    TOKEN_ONLY(TOKEN_EOF),
	};
	syntax_parser_t parser;
	assert_true(init_parser(&parser, tokens, sizeof(tokens) / sizeof(tokens[0])));
	assert_true(syntax_parse_program(&parser));
	assert_int_equal(count_action(SEMANTIC_ACTION_FUNCTION_NAME), 1);
	assert_int_equal(count_action(SEMANTIC_ACTION_RETURN), 1);
	assert_int_equal(count_action(SEMANTIC_ACTION_PROGRAM), 1);
	(void)state;
}

int main(void) {
	const struct CMUnitTest tests[] = {
	    cmocka_unit_test(parses_precedence_and_reduces_inner_operator_first),
	    cmocka_unit_test(rejects_chained_relational_operators),
	    cmocka_unit_test(recognizes_unit_and_nested_call_suffixes),
	    cmocka_unit_test(rejects_missing_right_operand),
	    cmocka_unit_test(accepts_complete_program_from_mock_tokens),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
