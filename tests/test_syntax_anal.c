#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cmocka.h>

#include "lexer.h"
#include "syntax_anal.h"

/* Build with the project sources and link against CMocka (-lcmocka). */

typedef struct ParserFixture {
	FILE *source;
	lexer_t lexer;
	syntax_parser_t parser;
} parser_fixture_t;

static size_t semantic_action_counts[SEMANTIC_ACTION_BLOCK_BEGIN + 1];
static size_t last_call_argument_count;

/* CMocka test double for the semantic analyzer's dispatcher. */
void syntax_semantic_action(syntax_semantic_action_t action, const token_t *token, size_t item_count) {
	(void)token;
	if ((size_t)action < sizeof(semantic_action_counts) / sizeof(semantic_action_counts[0])) {
		semantic_action_counts[action]++;
	}
	if (action == SEMANTIC_ACTION_CALL) {
		last_call_argument_count = item_count;
	}
}

static parser_fixture_t *fixture_create(const char *text) {
	parser_fixture_t *fixture = calloc(1, sizeof(*fixture));
	if (fixture == NULL) {
		return NULL;
	}
	fixture->source = tmpfile();
	if (fixture->source == NULL || fputs(text, fixture->source) == EOF || fflush(fixture->source) != 0 ||
	    fseek(fixture->source, 0, SEEK_SET) != 0) {
		if (fixture->source != NULL) {
			fclose(fixture->source);
		}
		free(fixture);
		return NULL;
	}
	if (lexer_init(&fixture->lexer, fixture->source) != LEXER_RESULT_OK) {
		lexer_destroy(&fixture->lexer);
		fclose(fixture->source);
		free(fixture);
		return NULL;
	}
	if (!syntax_parser_init(&fixture->parser, &fixture->lexer)) {
		lexer_destroy(&fixture->lexer);
		fclose(fixture->source);
		free(fixture);
		return NULL;
	}
	memset(semantic_action_counts, 0, sizeof(semantic_action_counts));
	last_call_argument_count = 0;
	return fixture;
}

static void fixture_destroy(parser_fixture_t *fixture) {
	if (fixture == NULL) {
		return;
	}
	lexer_destroy(&fixture->lexer);
	fclose(fixture->source);
	free(fixture);
}

static void expression_stops_before_newline(void **state) {
	parser_fixture_t *fixture = fixture_create("1 + 2 * 3\n");
	assert_non_null(fixture);
	assert_true(syntax_parse_expression(&fixture->parser));
	assert_int_equal(syntax_parser_current(&fixture->parser)->type, TOKEN_NEWLINE);
	assert_int_equal(semantic_action_counts[SEMANTIC_ACTION_PRIMARY], 3);
	assert_int_equal(semantic_action_counts[SEMANTIC_ACTION_BINARY], 2);
	fixture_destroy(fixture);
	(void)state;
}

static void expression_accepts_table_precedence(void **state) {
	parser_fixture_t *fixture = fixture_create("1 < 2 == 3 < 4 && true || false\n");
	assert_non_null(fixture);
	assert_true(syntax_parse_expression(&fixture->parser));
	assert_int_equal(syntax_parser_current(&fixture->parser)->type, TOKEN_NEWLINE);
	assert_int_equal(semantic_action_counts[SEMANTIC_ACTION_BINARY], 5);
	fixture_destroy(fixture);
	(void)state;
}

static void expression_rejects_chained_relational_operators(void **state) {
	parser_fixture_t *fixture = fixture_create("1 < 2 < 3\n");
	assert_non_null(fixture);
	assert_false(syntax_parse_expression(&fixture->parser));
	assert_true(fixture->parser.failed);
	fixture_destroy(fixture);
	(void)state;
}

static void expression_rejects_chained_equality_operators(void **state) {
	parser_fixture_t *fixture = fixture_create("1 == 2 != 3\n");
	assert_non_null(fixture);
	assert_false(syntax_parse_expression(&fixture->parser));
	assert_true(fixture->parser.failed);
	fixture_destroy(fixture);
	(void)state;
}

static void expression_handles_unit_member_and_nested_calls(void **state) {
	parser_fixture_t *unit = fixture_create("()\n");
	assert_non_null(unit);
	assert_true(syntax_parse_expression(&unit->parser));
	assert_int_equal(semantic_action_counts[SEMANTIC_ACTION_UNIT], 1);
	fixture_destroy(unit);

	parser_fixture_t *calls = fixture_create("foo.bar(1, nested(2))\n");
	assert_non_null(calls);
	assert_true(syntax_parse_expression(&calls->parser));
	assert_int_equal(syntax_parser_current(&calls->parser)->type, TOKEN_NEWLINE);
	assert_int_equal(semantic_action_counts[SEMANTIC_ACTION_MEMBER], 1);
	assert_int_equal(semantic_action_counts[SEMANTIC_ACTION_CALL], 2);
	assert_int_equal(last_call_argument_count, 2);
	fixture_destroy(calls);
	(void)state;
}

static void expression_rejects_trailing_call_comma(void **state) {
	parser_fixture_t *fixture = fixture_create("foo(1,)\n");
	assert_non_null(fixture);
	assert_false(syntax_parse_expression(&fixture->parser));
	assert_true(fixture->parser.failed);
	fixture_destroy(fixture);
	(void)state;
}

static void program_accepts_declaration_assignment_and_return(void **state) {
	const char *program = "import ifj26.*\n"
	                      "@main\n"
	                      "def main() =\n"
	                      "    val x: Int = 1\n"
	                      "    x = x + 2\n"
	                      "    return x\n";
	parser_fixture_t *fixture = fixture_create(program);
	assert_non_null(fixture);
	assert_true(syntax_parse_program(&fixture->parser));
	assert_int_equal(semantic_action_counts[SEMANTIC_ACTION_PROGRAM], 1);
	assert_int_equal(semantic_action_counts[SEMANTIC_ACTION_FUNCTION_NAME], 1);
	assert_int_equal(semantic_action_counts[SEMANTIC_ACTION_DECLARATION], 1);
	assert_int_equal(semantic_action_counts[SEMANTIC_ACTION_ASSIGNMENT], 1);
	assert_int_equal(semantic_action_counts[SEMANTIC_ACTION_RETURN], 1);
	fixture_destroy(fixture);
	(void)state;
}

static void program_rejects_wrong_prolog(void **state) {
	parser_fixture_t *fixture = fixture_create("import other.*\n");
	assert_non_null(fixture);
	assert_false(syntax_parse_program(&fixture->parser));
	assert_true(fixture->parser.failed);
	fixture_destroy(fixture);
	(void)state;
}

int main(void) {
	const struct CMUnitTest tests[] = {
	    cmocka_unit_test(expression_stops_before_newline),
	    cmocka_unit_test(expression_accepts_table_precedence),
	    cmocka_unit_test(expression_rejects_chained_relational_operators),
	    cmocka_unit_test(expression_rejects_chained_equality_operators),
	    cmocka_unit_test(expression_handles_unit_member_and_nested_calls),
	    cmocka_unit_test(expression_rejects_trailing_call_comma),
	    cmocka_unit_test(program_accepts_declaration_assignment_and_return),
	    cmocka_unit_test(program_rejects_wrong_prolog),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
