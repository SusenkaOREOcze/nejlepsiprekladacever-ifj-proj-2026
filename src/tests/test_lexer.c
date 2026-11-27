/**
 * @file test_lexer.c
 * @brief CMocka unit and stress tests for the IFJ26 lexer.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cmocka.h"

#include "lexer.h"
#include "lexer_internal.h"

/**
 * @brief One expected terminal in a lexer test sequence.
 */
typedef struct ExpectedToken {
	token_type_t type;
	const char *lexeme;
} expected_token_t;

/**
 * @brief Creates a rewinded temporary input stream from source text.
 * It creates a temporary file, writes the text to it, flushes, and rewinds.
 *
 * @param text IFJ26 source text.
 * @return Open temporary stream positioned at its beginning.
 */
static FILE *source_from_text(const char *text) {
	FILE *source = tmpfile();

	assert_non_null(source);
	assert_true(fputs(text, source) >= 0);
	assert_int_equal(fflush(source), 0);
	rewind(source);

	return source;
}

/**
 * @brief Compares the current lexer token against one expected token.
 *
 * @param lexer Lexer to use.
 * @param expected Expected token.
 */
static void assert_current_token(const lexer_t *lexer, expected_token_t expected) {
	const token_t *actual = lexer_current(lexer);

	assert_non_null(actual);
	assert_int_equal(actual->type, expected.type);

	if (expected.lexeme == NULL) {
		assert_null(actual->lexeme);
		assert_int_equal(actual->length, 0);
		return;
	}

	assert_non_null(actual->lexeme);
	assert_int_equal(actual->length, strlen(expected.lexeme));
	assert_string_equal(actual->lexeme, expected.lexeme);
}

/**
 * @brief Verifies the entire source stream against an expected token sequence.
 *
 * @param source_text Source text.
 * @param expected Expected terminals including TOKEN_EOF.
 * @param expected_count Number of expected terminals.
 */
static void assert_token_sequence(const char *source_text, const expected_token_t *expected,
                                  size_t expected_count) {
	FILE *source = source_from_text(source_text);
	lexer_t lexer;

	assert_int_equal(lexer_init(&lexer, source), LEXER_RESULT_OK);

	for (size_t index = 0; index < expected_count; ++index) {
		assert_current_token(&lexer, expected[index]);

		if (index + 1 < expected_count) {
			assert_int_equal(lexer_advance(&lexer), LEXER_RESULT_OK);
		}
	}

	lexer_destroy(&lexer);
	assert_int_equal(fclose(source), 0);
}

/**
 * @brief Verifies that scanning source eventually produces a lexical error.
 *
 * Lexer initialization prepares both current and next token, so invalid source
 * can fail either during initialization or during a later advance.
 *
 * @param source_text Invalid IFJ26 source text.
 */
static void assert_lexical_error(const char *source_text) {
	FILE *source = source_from_text(source_text);
	lexer_t lexer;
	lexer_result_t result = lexer_init(&lexer, source);

	if (result == LEXER_RESULT_OK) {
		for (;;) {
			result = lexer_advance(&lexer);

			if (result != LEXER_RESULT_OK) {
				break;
			}

			assert_true(lexer_current(&lexer)->type != TOKEN_EOF);
		}
	}

	assert_int_equal(result, LEXER_RESULT_LEXICAL_ERROR);
	assert_non_null(lexer_error(&lexer));
	assert_int_equal(lexer_error(&lexer)->code, LEXER_RESULT_LEXICAL_ERROR);

	lexer_destroy(&lexer);
	assert_int_equal(fclose(source), 0);
}

// =============================================================================
// Category: public API
// =============================================================================

/**
 * @brief Example: `alpha beta`; checks current/next and advance.
 */
static void test_api_current_next_and_advance(void **state) {
	(void)state;

	FILE *source = source_from_text("alpha beta gamma\n");
	lexer_t lexer;

	assert_int_equal(lexer_init(&lexer, source), LEXER_RESULT_OK);
	assert_current_token(&lexer, (expected_token_t){TOKEN_IDENTIFIER, "alpha"});
	assert_int_equal(lexer_next(&lexer)->type, TOKEN_IDENTIFIER);
	assert_string_equal(lexer_next(&lexer)->lexeme, "beta");

	assert_int_equal(lexer_advance(&lexer), LEXER_RESULT_OK);
	assert_current_token(&lexer, (expected_token_t){TOKEN_IDENTIFIER, "beta"});
	assert_int_equal(lexer_next(&lexer)->type, TOKEN_IDENTIFIER);
	assert_string_equal(lexer_next(&lexer)->lexeme, "gamma");

	assert_int_equal(lexer_advance(&lexer), LEXER_RESULT_OK);
	assert_current_token(&lexer, (expected_token_t){TOKEN_IDENTIFIER, "gamma"});
	assert_int_equal(lexer_next(&lexer)->type, TOKEN_NEWLINE);

	lexer_destroy(&lexer);
	assert_int_equal(fclose(source), 0);
}

/**
 * @brief Example: empty input; checks current and next are EOF.
 */
static void test_api_empty_input_contains_eof(void **state) {
	(void)state;

	FILE *source = source_from_text("");
	lexer_t lexer;

	assert_int_equal(lexer_init(&lexer, source), LEXER_RESULT_OK);
	assert_int_equal(lexer_current(&lexer)->type, TOKEN_EOF);
	assert_int_equal(lexer_next(&lexer)->type, TOKEN_EOF);

	lexer_destroy(&lexer);
	assert_int_equal(fclose(source), 0);
}

// =============================================================================
// Category: identifiers and reserved words
// =============================================================================

/**
 * @brief Example: all keywords, `_`, `ifElse`; checks exact token.
 */
static void test_identifiers_all_keywords_literals(void **state) {
	(void)state;

	static const expected_token_t expected[] = {
	    {TOKEN_DEF, "def"},
	    {TOKEN_DO, "do"},
	    {TOKEN_ELSE, "else"},
	    {TOKEN_IF, "if"},
	    {TOKEN_IMPORT, "import"},
	    {TOKEN_RETURN, "return"},
	    {TOKEN_THEN, "then"},
	    {TOKEN_VAL, "val"},
	    {TOKEN_VAR, "var"},
	    {TOKEN_WHILE, "while"},
	    {TOKEN_TYPE_INT, "Int"},
	    {TOKEN_TYPE_DOUBLE, "Double"},
	    {TOKEN_TYPE_STRING, "String"},
	    {TOKEN_TYPE_UNIT, "Unit"},
	    {TOKEN_TYPE_BOOLEAN, "Boolean"},
	    {TOKEN_TYPE_LIST, "List"},
	    {TOKEN_CASE, "case"},
	    {TOKEN_CLASS, "class"},
	    {TOKEN_EXTENSION, "extension"},
	    {TOKEN_FOR, "for"},
	    {TOKEN_MATCH, "match"},
	    {TOKEN_TYPE, "type"},
	    {TOKEN_TRUE, "true"},
	    {TOKEN_FALSE, "false"},
	    {TOKEN_WILDCARD, "_"},
	    {TOKEN_IDENTIFIER, "ifElse"},
	    {TOKEN_IDENTIFIER, "trueValue"},
	    {TOKEN_IDENTIFIER, "value_42"},
	    {TOKEN_NEWLINE, NULL},
	    {TOKEN_EOF, NULL},
	};

	assert_token_sequence(
	    "def do else if import return then val var while Int Double String Unit Boolean List "
	    "case class extension for match type true false _ ifElse trueValue value_42\n",
	    expected, sizeof(expected) / sizeof(expected[0]));
}

/**
 * @brief Example: `_name name_2 IF Int int`; checks identifier boundaries, and
 * case-sensitive keywords.
 */
static void test_identifiers_boundaries_and_case_sensitivity(void **state) {
	(void)state;

	static const expected_token_t expected[] = {
	    {TOKEN_IDENTIFIER, "_name"},
	    {TOKEN_IDENTIFIER, "name_2"},
	    {TOKEN_IDENTIFIER, "IF"},
	    {TOKEN_TYPE_INT, "Int"},
	    {TOKEN_IDENTIFIER, "int"},
	    {TOKEN_NEWLINE, NULL},
	    {TOKEN_EOF, NULL},
	};

	assert_token_sequence("_name name_2 IF Int int\n", expected, sizeof(expected) / sizeof(expected[0]));
}

/**
 * @brief Example: `@main`; checks the exact annotation token.
 */
static void test_identifiers_main_annotation(void **state) {
	(void)state;

	static const expected_token_t valid[] = {
	    {TOKEN_MAIN_ANNOTATION, "@main"},
	    {TOKEN_NEWLINE, NULL},
	    {TOKEN_EOF, NULL},
	};

	assert_token_sequence("@main\n", valid, sizeof(valid) / sizeof(valid[0]));
}

/**
 * @brief Example: `@mainFunction`; checks an identifier cannot extend @main.
 */
static void test_identifiers_reject_main_annotation_suffix(void **state) {
	(void)state;

	assert_lexical_error("@mainFunction\n");
	assert_lexical_error("@mains\n");
}

/**
 * @brief Example: `@entry`; checks unknown annotations are rejected.
 */
static void test_identifiers_reject_unknown_annotation(void **state) {
	(void)state;

	assert_lexical_error("@entry\n");
}

/**
 * @brief Example: `@`; checks annotation prefix without a name is rejected.
 */
static void test_identifiers_reject_bare_at_sign(void **state) {
	(void)state;

	assert_lexical_error("@\n");
}

/**
 * @brief Example: `@mai`; checks truncated @main annotation is rejected.
 */
static void test_identifiers_reject_incomplete_main_annotation(void **state) {
	(void)state;

	assert_lexical_error("@mai\n");
}

// =============================================================================
// Category: punctuation and operators
// =============================================================================

/**
 * @brief Example: `=>`, `<=`, `&&`; checks every punctuation and operator.
 */
static void test_symbols_every_supported_terminal(void **state) {
	(void)state;

	static const expected_token_t expected[] = {
	    {TOKEN_LPAREN, "("},
	    {TOKEN_RPAREN, ")"},
	    {TOKEN_LBRACKET, "["},
	    {TOKEN_RBRACKET, "]"},
	    {TOKEN_COMMA, ","},
	    {TOKEN_SEMICOLON, ";"},
	    {TOKEN_DOT, "."},
	    {TOKEN_COLON, ":"},
	    {TOKEN_ASSIGN, "="},
	    {TOKEN_FAT_ARROW, "=>"},
	    {TOKEN_LEFT_ARROW, "<-"},
	    {TOKEN_PIPE, "|"},
	    {TOKEN_PLUS, "+"},
	    {TOKEN_CONCAT, "++"},
	    {TOKEN_MINUS, "-"},
	    {TOKEN_MULTIPLY, "*"},
	    {TOKEN_DIVIDE, "/"},
	    {TOKEN_LESS, "<"},
	    {TOKEN_LESS_EQUAL, "<="},
	    {TOKEN_GREATER, ">"},
	    {TOKEN_GREATER_EQUAL, ">="},
	    {TOKEN_EQUAL, "=="},
	    {TOKEN_NOT_EQUAL, "!="},
	    {TOKEN_NOT, "!"},
	    {TOKEN_AND, "&&"},
	    {TOKEN_OR, "||"},
	    {TOKEN_NEWLINE, NULL},
	    {TOKEN_EOF, NULL},
	};

	assert_token_sequence("()[],;.: = => <- | + ++ - * / < <= > >= == != ! && ||\n", expected,
	                      sizeof(expected) / sizeof(expected[0]));
}

/**
 * @brief Example: `&`; checks incomplete paired operator is rejected.
 */
static void test_symbols_reject_incomplete_ampersand(void **state) {
	(void)state;

	assert_lexical_error("&\n");
	assert_lexical_error("value & next\n");
}

/**
 * @brief Example: `+++ ||| && == === <---`; checks longest valid operator match, then tokenization of remaining characters.
 */
static void test_symbols_pair_prefixes_and_repeated_operators(void **state) {
	(void)state;

	static const expected_token_t expected[] = {
	    {TOKEN_CONCAT, "++"},
	    {TOKEN_PLUS, "+"},
	    {TOKEN_OR, "||"},
	    {TOKEN_PIPE, "|"},
	    {TOKEN_AND, "&&"},
	    {TOKEN_EQUAL, "=="},
	    {TOKEN_EQUAL, "=="},
	    {TOKEN_ASSIGN, "="},
	    {TOKEN_LEFT_ARROW, "<-"},
	    {TOKEN_MINUS, "-"},
	    {TOKEN_MINUS, "-"},
	    {TOKEN_NEWLINE, NULL},
	    {TOKEN_EOF, NULL},
	};

	assert_token_sequence("+++ ||| && == === <---\n", expected, sizeof(expected) / sizeof(expected[0]));
}

// =============================================================================
// Category: number literals
// =============================================================================

/** @brief Example: `1_000`, `.5`, `4e2`; checks valid numeric forms. */
static void test_numbers_valid_forms(void **state) {
	(void)state;

	static const expected_token_t expected[] = {
	    {TOKEN_INTEGER, "0"},
	    {TOKEN_INTEGER, "42"},
	    {TOKEN_INTEGER, "1_000"},
	    {TOKEN_INTEGER, "1__000"}, // this thing is valid number :devastated:
	    {TOKEN_DOUBLE, ".5"},
	    {TOKEN_DOUBLE, "2.5"},
	    {TOKEN_DOUBLE, "4e2"},
	    {TOKEN_DOUBLE, "6E-3"},
	    {TOKEN_DOUBLE, "7.0D"},
	    {TOKEN_DOUBLE, "8d"},
	    {TOKEN_NEWLINE, NULL},
	    {TOKEN_EOF, NULL},
	};

	assert_token_sequence("0 42 1_000 1__000 .5 2.5 4e2 6E-3 7.0D 8d\n", expected,
	                      sizeof(expected) / sizeof(expected[0]));
}

/**
 * @brief Example: `01`, `0_1`; checks leading zeroes are rejected.
 */
static void test_numbers_reject_leading_zero(void **state) {
	(void)state;

	assert_lexical_error("01\n");
	assert_lexical_error("0_1\n");
}

/**
 * @brief Example: `1_`, `1e_2`, `1._2`; checks misplaced separators are rejected.
 */
static void test_numbers_reject_misplaced_separator(void **state) {
	(void)state;

	assert_lexical_error("1_\n");
	assert_lexical_error("1e_2\n");
	assert_lexical_error("1._2\n");
}

/**
 * @brief Example: `1e`, `1e+`, `1e--2`; checks incomplete exponents are rejected.
 */
static void test_numbers_reject_incomplete_exponent(void **state) {
	(void)state;

	assert_lexical_error("1e\n");
	assert_lexical_error("1e+\n");
	assert_lexical_error("1e--2\n");
}

/**
 * @brief Example: `12name`, `1d2`; checks identifiers cannot follow numbers directly.
 */
static void test_numbers_reject_identifier_suffix(void **state) {
	(void)state;

	assert_lexical_error("12name\n");
	assert_lexical_error("1d2\n");
}

/**
 * @brief Example: `1__2`, `3_4.5__6`, `7__8e+9__0`, `1_2D`; checks underscore runs between digits in integer, fraction, and exponent components.
 */
static void test_numbers_separators_in_each_component(void **state) {
	(void)state;

	static const expected_token_t expected[] = {
	    {TOKEN_INTEGER, "1__2"},
	    {TOKEN_DOUBLE, "3_4.5__6"},
	    {TOKEN_DOUBLE, "7__8e+9__0"},
	    {TOKEN_DOUBLE, "1_2D"},
	    {TOKEN_NEWLINE, NULL},
	    {TOKEN_EOF, NULL},
	};

	assert_token_sequence("1__2 3_4.5__6 7__8e+9__0 1_2D\n", expected,
	                      sizeof(expected) / sizeof(expected[0]));
}

/**
 * @brief Example: `0.0`, `0e0`, `2E-3`, `4d`, `5.6D`; checks zero-valued forms, signed exponent, and optional D suffix.
 */
static void test_numbers_double_forms_and_boundaries(void **state) {
	(void)state;

	static const expected_token_t expected[] = {
	    {TOKEN_DOUBLE, "0.0"},
	    {TOKEN_DOUBLE, "0e0"},
	    {TOKEN_DOUBLE, "2E-3"},
	    {TOKEN_DOUBLE, "4d"},
	    {TOKEN_DOUBLE, "5.6D"},
	    {TOKEN_NEWLINE, NULL},
	    {TOKEN_EOF, NULL},
	};

	assert_token_sequence("0.0 0e0 2E-3 4d 5.6D\n", expected, sizeof(expected) / sizeof(expected[0]));
}

/**
 * @brief Example: `0__1`; checks that a multi-digit integer cannot start with zero.
 */
static void test_numbers_reject_zero_with_repeated_separators(void **state) {
	(void)state;

	assert_lexical_error("0__1\n");
}

/**
 * @brief Example: `2e-`; checks that an exponent sign must be followed by digits.
 */
static void test_numbers_reject_exponent_without_digits_after_sign(void **state) {
	(void)state;

	assert_lexical_error("2e-\n");
}

/**
 * @brief Example: `2.`; checks that a decimal point must be followed by fractional digits.
 */
static void test_numbers_reject_fraction_without_digits(void **state) {
	(void)state;

	assert_lexical_error("2.\n");
}

/**
 * @brief Example: `12_e3`; checks that an underscore cannot touch the exponent marker.
 */
static void test_numbers_reject_separator_before_exponent(void **state) {
	(void)state;

	assert_lexical_error("12_e3\n");
}

// =============================================================================
// Category: strings
// =============================================================================

/**
 * @brief Example: `"plain"`, `"\\n"`; checks token spelling and escapes.
 */
static void test_strings_normal_escapes(void **state) {
	(void)state;

	static const expected_token_t expected[] = {
	    {TOKEN_STRING, "\"plain\""},
	    {TOKEN_STRING, "\"\\\\\\\"\\n\\r\\t\""}, // Literal: "\\\"\n\r\t"
	    {TOKEN_NEWLINE, NULL},
	    {TOKEN_EOF, NULL},
	};

	assert_token_sequence("\"plain\" \"\\\\\\\"\\n\\r\\t\"\n", expected,
	                      sizeof(expected) / sizeof(expected[0]));
}

/**
 * @brief Example: `"""a\nb\\n"""`; checks raw multiline content.
 */
static void test_strings_multiline_is_one_raw_token(void **state) {
	(void)state;

	static const expected_token_t expected[] = {
	    {TOKEN_MULTILINE_STRING, "\"\"\"first\nsecond\\nthird\"\"\""}, // Literal: """first\nsecond\nthird"""
	    {TOKEN_NEWLINE, NULL},
	    {TOKEN_EOF, NULL},
	};

	assert_token_sequence("\"\"\"first\nsecond\\nthird\"\"\"\n", expected,
	                      sizeof(expected) / sizeof(expected[0]));
}

/**
 * @brief Example: `"\\q"`; checks an unknown escape is rejected.
 */
static void test_strings_reject_invalid_escape(void **state) {
	(void)state;

	assert_lexical_error("\"bad\\q\"\n");
}

/**
 * @brief Example: `"line break\n`; checks a newline cannot end an ordinary string.
 */
static void test_strings_reject_unclosed_ordinary(void **state) {
	(void)state;

	assert_lexical_error("\"line break\n");
}

/**
 * @brief Example: `"""unclosed`; checks a multiline string requires closing quotes.
 */
static void test_strings_reject_unclosed_multiline(void **state) {
	(void)state;

	assert_lexical_error("\"\"\"unclosed\ntext");
}

/**
 * @brief Example: `"" """"""`; checks empty ordinary and multiline strings.
 */
static void test_strings_empty_literals(void **state) {
	(void)state;

	static const expected_token_t expected[] = {
	    {TOKEN_STRING, "\"\""},
	    {TOKEN_MULTILINE_STRING, "\"\"\"\"\"\""},
	    {TOKEN_NEWLINE, NULL},
	    {TOKEN_EOF, NULL},
	};

	assert_token_sequence("\"\" \"\"\"\"\"\"\n", expected, sizeof(expected) / sizeof(expected[0]));
}

/**
 * @brief Example: `"x""y"`; checks adjacent strings remain separate tokens.
 */
static void test_strings_adjacent_literals(void **state) {
	(void)state;

	static const expected_token_t expected[] = {
	    {TOKEN_STRING, "\"x\""},
	    {TOKEN_STRING, "\"y\""},
	    {TOKEN_NEWLINE, NULL},
	    {TOKEN_EOF, NULL},
	};

	assert_token_sequence("\"x\"\"y\"\n", expected, sizeof(expected) / sizeof(expected[0]));
}

/**
 * @brief Example: `"""a""b""" then next`; checks fewer than three quotes stay in the body and following token location advances by LF.
 */
static void test_strings_multiline_quotes_and_following_line(void **state) {
	(void)state;

	FILE *source = source_from_text("\"\"\"a\"\"b\"\"\"\nnext\n");
	lexer_t lexer;

	assert_int_equal(lexer_init(&lexer, source), LEXER_RESULT_OK);
	assert_current_token(&lexer, (expected_token_t){TOKEN_MULTILINE_STRING, "\"\"\"a\"\"b\"\"\""});
	assert_int_equal(lexer_advance(&lexer), LEXER_RESULT_OK);
	assert_int_equal(lexer_current(&lexer)->type, TOKEN_NEWLINE);
	assert_int_equal(lexer_advance(&lexer), LEXER_RESULT_OK);
	assert_current_token(&lexer, (expected_token_t){TOKEN_IDENTIFIER, "next"});
	assert_int_equal(lexer_current(&lexer)->location.line, 2);

	lexer_destroy(&lexer);
	assert_int_equal(fclose(source), 0);
}

/**
 * @brief Example: `"abc\<EOF>`; checks a backslash without an escape character is rejected.
 */
static void test_strings_reject_escape_at_end_of_input(void **state) {
	(void)state;

	assert_lexical_error("\"abc\\");
}

/**
 * @brief Example: `"a\rb"`; checks a carriage return is rejected inside an ordinary string.
 */
static void test_strings_reject_carriage_return(void **state) {
	(void)state;

	assert_lexical_error("\"a\rb\"\n");
}

// =============================================================================
// Tests: comments, logical lines, and continuation
// =============================================================================

/**
 * @brief Example: `// c`; checks line and nested block comments are skipped.
 */
static void test_comments_skip_ignored_text(void **state) {
	(void)state;

	static const expected_token_t expected[] = {
	    {TOKEN_IDENTIFIER, "first"},
	    {TOKEN_NEWLINE, NULL},
	    {TOKEN_IDENTIFIER, "second"},
	    {TOKEN_IDENTIFIER, "third"},
	    {TOKEN_NEWLINE, NULL},
	    {TOKEN_IDENTIFIER, "last"},
	    {TOKEN_NEWLINE, NULL},
	    {TOKEN_EOF, NULL},
	};

	assert_token_sequence(
	    "\n  // comment-only line\nfirst // ignored\nsecond /* outer /* inner */ outer */ third\nlast\n",
	    expected, sizeof(expected) / sizeof(expected[0]));
}

/**
 * @brief Example: token, multiline comment, next line; checks the logical line boundary.
 */
static void test_comments_multiline_comment_ends_logical_line(void **state) {
	(void)state;

	static const expected_token_t expected[] = {
	    {TOKEN_IDENTIFIER, "value"},
	    {TOKEN_NEWLINE, NULL},
	    {TOKEN_IDENTIFIER, "next"},
	    {TOKEN_NEWLINE, NULL},
	    {TOKEN_EOF, NULL},
	};

	assert_token_sequence("value /* first\nsecond */  \nnext\n", expected,
	                      sizeof(expected) / sizeof(expected[0]));
}

/**
 * @brief Example: `left /\* c *\/ right // tail`; checks a one-line block comment separates tokens and a line comment preserves the newline.
 */
static void test_comments_single_line_separate_adjacent_tokens(void **state) {
	(void)state;

	static const expected_token_t expected[] = {
	    {TOKEN_IDENTIFIER, "left"},
	    {TOKEN_IDENTIFIER, "right"},
	    {TOKEN_NEWLINE, NULL},
	    {TOKEN_EOF, NULL},
	};

	assert_token_sequence("left/* c */right // tail\n", expected, sizeof(expected) / sizeof(expected[0]));
}

/**
 * @brief Example: `// comment at EOF`; checks a line comment ends at EOF.
 */
static void test_comments_line_comment_at_eof(void **state) {
	(void)state;

	assert_token_sequence("// comment at EOF", (expected_token_t[]){{TOKEN_EOF, NULL}}, 1);
}

/**
 * @brief Example: `a /\* outer\n /\* inner *\/ end *\/\nb`; Checks: nested comment closure and logical line tokens around ignored comment-only lines.
 */
static void test_comments_nested_multiline_and_comment_only_lines(void **state) {
	(void)state;

	static const expected_token_t expected[] = {
	    {TOKEN_IDENTIFIER, "a"},
	    {TOKEN_NEWLINE, NULL},
	    {TOKEN_IDENTIFIER, "b"},
	    {TOKEN_NEWLINE, NULL},
	    {TOKEN_EOF, NULL},
	};

	assert_token_sequence("a /* outer\n /* inner */ end */\nb\n", expected,
	                      sizeof(expected) / sizeof(expected[0]));
}

/**
 * @brief Example: `call(\n  value // comment\n  )`; checks comments do not end a continuation.
 */
static void test_layout_parentheses_suppress_commented_newline(void **state) {
	(void)state;

	static const expected_token_t expected[] = {
	    {TOKEN_IDENTIFIER, "call"},
	    {TOKEN_LPAREN, "("},
	    {TOKEN_IDENTIFIER, "value"},
	    {TOKEN_RPAREN, ")"},
	    {TOKEN_NEWLINE, NULL},
	    {TOKEN_EOF, NULL},
	};

	assert_token_sequence("call(\n  value // comment\n  )\n", expected,
	                      sizeof(expected) / sizeof(expected[0]));
}

/**
 * @brief Example: `call(\n  [\n    x,\n    y\n  ]\n  )`; checks nested parentheses and brackets suppress
 * physical NEWLINE tokens.
 */
static void test_layout_nested_brackets_suppress_physical_newlines(void **state) {
	(void)state;

	static const expected_token_t expected[] = {
	    {TOKEN_IDENTIFIER, "call"},
	    {TOKEN_LPAREN, "("},
	    {TOKEN_LBRACKET, "["},
	    {TOKEN_IDENTIFIER, "x"},
	    {TOKEN_COMMA, ","},
	    {TOKEN_IDENTIFIER, "y"},
	    {TOKEN_RBRACKET, "]"},
	    {TOKEN_RPAREN, ")"},
	    {TOKEN_NEWLINE, NULL},
	    {TOKEN_EOF, NULL},
	};

	assert_token_sequence("call(\n  [\n    x,\n    y\n  ]\n  )\n", expected,
	                      sizeof(expected) / sizeof(expected[0]));
}

/**
 * @brief Example: `root\n  child<EOF>`; checks EOF emits pending DEDENT before TOKEN_EOF without a final LF.
 */
static void test_layout_eof_without_final_newline_closes_blocks(void **state) {
	(void)state;

	static const expected_token_t expected[] = {
	    {TOKEN_IDENTIFIER, "root"},
	    {TOKEN_NEWLINE, NULL},
	    {TOKEN_INDENT, NULL},
	    {TOKEN_IDENTIFIER, "child"},
	    {TOKEN_DEDENT, NULL},
	    {TOKEN_EOF, NULL},
	};

	assert_token_sequence("root\n  child", expected, sizeof(expected) / sizeof(expected[0]));
}

/**
 * @brief Example: ` \tvalue`; checks a leading tab is rejected even when preceded by spaces.
 */
static void test_layout_rejects_tab_after_leading_spaces(void **state) {
	(void)state;

	assert_lexical_error(" \tvalue\n");
}

/**
 * @brief Example: indentation widths 0 -> 4 -> 2; checks dedent must match an earlier width exactly.
 */
static void test_layout_rejects_dedent_to_unseen_width(void **state) {
	(void)state;

	assert_lexical_error("root\n    child\n  sibling\n");
}

/**
 * @brief Example: `call(\nvalue\n  )`; checks continuation line must indent past the logical line.
 */
static void test_layout_rejects_unindented_parenthesis_continuation(void **state) {
	(void)state;

	assert_lexical_error("call(\nvalue\n  )\n");
}

/**
 * @brief Example: `value \/\* first\nsecond *\/ next`; checks a multiline block comment must end its logical
 * line.
 */
static void test_comments_reject_code_after_multiline_comment(void **state) {
	(void)state;

	assert_lexical_error("value /* first\nsecond */ next\n");
}

/**
 * @brief Example: `/\* never closes`; checks an unfinished block comment is rejected.
 */
static void test_comments_reject_unclosed_block_comment(void **state) {
	(void)state;

	assert_lexical_error("/* never closes");
}

/**
 * @brief Example: `/\* outer /\* inner *\/`; checks closing the inner comment leaves the outer comment open.
 */
static void test_comments_reject_unterminated_nested_comment(void **state) {
	(void)state;

	assert_lexical_error("/* outer /* inner */\n");
}

/**
 * @brief Example: `// \xC3\xA1`; checks non-ASCII bytes are rejected inside comments.
 */
static void test_comments_reject_non_ascii_comment_byte(void **state) {
	(void)state;

	assert_lexical_error("// \xC3\xA1\n");
}

// =============================================================================
// Tests: indentation and layout
// =============================================================================

/**
 * @brief Example: `root\n  child\n`; checks INDENT and EOF DEDENT.
 */
static void test_layout_single_indent_and_eof_dedent(void **state) {
	(void)state;

	static const expected_token_t expected[] = {
	    {TOKEN_IDENTIFIER, "root"},
	    {TOKEN_NEWLINE, NULL},
	    {TOKEN_INDENT, NULL},
	    {TOKEN_IDENTIFIER, "child"},
	    {TOKEN_NEWLINE, NULL},
	    {TOKEN_DEDENT, NULL},
	    {TOKEN_EOF, NULL},
	};

	assert_token_sequence("root\n  child\n", expected, sizeof(expected) / sizeof(expected[0]));
}

/**
 * @brief Example: indentation 0→2→4→0; checks consecutive DEDENTs.
 */
static void test_layout_multiple_dedents_on_one_line(void **state) {
	(void)state;

	static const expected_token_t expected[] = {
	    {TOKEN_IDENTIFIER, "top"},
	    {TOKEN_NEWLINE, NULL},
	    {TOKEN_INDENT, NULL},
	    {TOKEN_IDENTIFIER, "middle"},
	    {TOKEN_NEWLINE, NULL},
	    {TOKEN_INDENT, NULL},
	    {TOKEN_IDENTIFIER, "bottom"},
	    {TOKEN_NEWLINE, NULL},
	    {TOKEN_DEDENT, NULL},
	    {TOKEN_DEDENT, NULL},
	    {TOKEN_IDENTIFIER, "again"},
	    {TOKEN_NEWLINE, NULL},
	    {TOKEN_EOF, NULL},
	};

	assert_token_sequence("top\n  middle\n    bottom\nagain\n", expected,
	                      sizeof(expected) / sizeof(expected[0]));
}

/**
 * @brief Example: `\tvalue`; checks a leading tab before code is rejected.
 */
static void test_layout_rejects_leading_tab(void **state) {
	(void)state;

	assert_lexical_error("\tvalue\n");
	assert_lexical_error(" \t/* inline */ value\n");
}

/**
 * @brief Example: two spaces, comment, token; checks only the prefix sets indentation.
 */
static void test_layout_ignores_whitespace_after_single_line_block_comment(void **state) {
	(void)state;

	static const expected_token_t expected[] = {
	    {TOKEN_IDENTIFIER, "root"},
	    {TOKEN_NEWLINE, NULL},
	    {TOKEN_INDENT, NULL},
	    {TOKEN_IDENTIFIER, "child"},
	    {TOKEN_NEWLINE, NULL},
	    {TOKEN_IDENTIFIER, "sibling"},
	    {TOKEN_NEWLINE, NULL},
	    {TOKEN_DEDENT, NULL},
	    {TOKEN_EOF, NULL},
	};

	assert_token_sequence("root\n   /* comment */ child\n   sibling\n", expected,
	                      sizeof(expected) / sizeof(expected[0]));
}

/**
 * @brief Example: `root\n/* c *\/   child\nchild2`; checks spaces after a leading comment do not create
 * indentation.
 */
static void test_layout_comment_before_code_without_leading_spaces(void **state) {
	(void)state;

	static const expected_token_t expected[] = {
	    {TOKEN_IDENTIFIER, "root"},
	    {TOKEN_NEWLINE, NULL},
	    {TOKEN_IDENTIFIER, "child"},
	    {TOKEN_NEWLINE, NULL},
	    {TOKEN_IDENTIFIER, "child2"},
	    {TOKEN_NEWLINE, NULL},
	    {TOKEN_EOF, NULL},
	};

	assert_token_sequence("root\n/* c */   child\nchild2\n", expected,
	                      sizeof(expected) / sizeof(expected[0]));
}

/**
 * @brief Example: `root\n/* c *\/\tchild\nsibling`; checks a tab after a comment separates tokens but does
 * not create indentation.
 */
static void test_layout_tab_after_comment_is_separating_whitespace(void **state) {
	(void)state;

	static const expected_token_t expected[] = {
	    {TOKEN_IDENTIFIER, "root"},
	    {TOKEN_NEWLINE, NULL},
	    {TOKEN_INDENT, NULL},
	    {TOKEN_IDENTIFIER, "child"},
	    {TOKEN_NEWLINE, NULL},
	    {TOKEN_IDENTIFIER, "sibling"},
	    {TOKEN_NEWLINE, NULL},
	    {TOKEN_DEDENT, NULL},
	    {TOKEN_EOF, NULL},
	};

	assert_token_sequence("root\n /* c */\tchild\n sibling\n", expected,
	                      sizeof(expected) / sizeof(expected[0]));
}

/**
 * @brief Example: `root\n  /*c*\/x\n  /*c*\/ x\n  /*c*\/    x`; checks comment spacing does not affect
 * indentation.
 */
static void test_layout_comment_spacing_matrix(void **state) {
	(void)state;

	assert_token_sequence("root\n  /*c*/x\n  /*c*/ x\n  /*c*/    x\n",
	                      (expected_token_t[]){{TOKEN_IDENTIFIER, "root"},
	                                           {TOKEN_NEWLINE, NULL},
	                                           {TOKEN_INDENT, NULL},
	                                           {TOKEN_IDENTIFIER, "x"},
	                                           {TOKEN_NEWLINE, NULL},
	                                           {TOKEN_IDENTIFIER, "x"},
	                                           {TOKEN_NEWLINE, NULL},
	                                           {TOKEN_IDENTIFIER, "x"},
	                                           {TOKEN_NEWLINE, NULL},
	                                           {TOKEN_DEDENT, NULL},
	                                           {TOKEN_EOF, NULL}},
	                      11);
}

/**
 * @brief Example: `root\n  child\n\n  // comment\n  /* block *\/\n  next`; checks blank and comment-only
 * lines preserve the active indentation level.
 */
static void test_layout_blank_lines_do_not_close_indentation(void **state) {
	(void)state;

	static const expected_token_t expected[] = {
	    {TOKEN_IDENTIFIER, "root"},
	    {TOKEN_NEWLINE, NULL},
	    {TOKEN_INDENT, NULL},
	    {TOKEN_IDENTIFIER, "child"},
	    {TOKEN_NEWLINE, NULL},
	    {TOKEN_IDENTIFIER, "next"},
	    {TOKEN_NEWLINE, NULL},
	    {TOKEN_DEDENT, NULL},
	    {TOKEN_EOF, NULL},
	};

	assert_token_sequence("root\n  child\n\n  // comment\n  /* block */\n  next\n", expected,
	                      sizeof(expected) / sizeof(expected[0]));
}

/**
 * @brief Example: `a\n  b\n    c\n/* comment *\/ d\n`; checks comment is ignored while both closing DEDENT
 * tokens are emitted.
 */
static void test_layout_multiple_dedents_after_comment_line(void **state) {
	(void)state;

	static const expected_token_t expected[] = {
	    {TOKEN_IDENTIFIER, "a"},
	    {TOKEN_NEWLINE, NULL},
	    {TOKEN_INDENT, NULL},
	    {TOKEN_IDENTIFIER, "b"},
	    {TOKEN_NEWLINE, NULL},
	    {TOKEN_INDENT, NULL},
	    {TOKEN_IDENTIFIER, "c"},
	    {TOKEN_NEWLINE, NULL},
	    {TOKEN_DEDENT, NULL},
	    {TOKEN_DEDENT, NULL},
	    {TOKEN_IDENTIFIER, "d"},
	    {TOKEN_NEWLINE, NULL},
	    {TOKEN_EOF, NULL},
	};

	assert_token_sequence("a\n  b\n    c\n/* comment */ d\n", expected,
	                      sizeof(expected) / sizeof(expected[0]));
}

/**
 * @brief Example: tab-only and comment-only lines; checks no indentation token.
 */
static void test_layout_ignores_tabs_on_blank_lines(void **state) {
	(void)state;

	static const expected_token_t expected[] = {
	    {TOKEN_IDENTIFIER, "root"},
	    {TOKEN_NEWLINE, NULL},
	    {TOKEN_EOF, NULL},
	};

	assert_token_sequence("\t\n  \t\n\t// comment\n  /* comment */\t\nroot\n", expected,
	                      sizeof(expected) / sizeof(expected[0]));

	static const expected_token_t eof_only[] = {{TOKEN_EOF, NULL}};
	assert_token_sequence("\t", eof_only, sizeof(eof_only) / sizeof(eof_only[0]));
	assert_token_sequence("  \t/* comment */", eof_only, sizeof(eof_only) / sizeof(eof_only[0]));

	static const expected_token_t trailing_blank[] = {
	    {TOKEN_IDENTIFIER, "root"},
	    {TOKEN_NEWLINE, NULL},
	    {TOKEN_EOF, NULL},
	};
	assert_token_sequence("root\n\t", trailing_blank, sizeof(trailing_blank) / sizeof(trailing_blank[0]));
}

/**
 * @brief Example: `\t/\* comment *\/value`; checks a leading tab before code is rejected.
 */
static void test_layout_rejects_tab_before_comment_and_code(void **state) {
	(void)state;

	assert_lexical_error("\t/* comment */value");
}

/**
 * @brief Example: `root\r\n`; checks CRLF is rejected.
 */
static void test_layout_rejects_crlf(void **state) {
	(void)state;

	assert_lexical_error("root\r\n");
	assert_lexical_error("\r\n");
}

// =============================================================================
// Tests: token locations, defensive checks, and invalid input
// =============================================================================

/**
 * @brief Example: `first\n  second`; checks token line and column.
 */
static void test_boundaries_token_locations(void **state) {
	(void)state;

	FILE *source = source_from_text("first\n  second\n");
	lexer_t lexer;

	assert_int_equal(lexer_init(&lexer, source), LEXER_RESULT_OK);
	assert_int_equal(lexer_current(&lexer)->location.line, 1);
	assert_int_equal(lexer_current(&lexer)->location.column, 1);

	assert_int_equal(lexer_advance(&lexer), LEXER_RESULT_OK);
	assert_int_equal(lexer_current(&lexer)->type, TOKEN_NEWLINE);
	assert_int_equal(lexer_current(&lexer)->location.line, 1);
	assert_int_equal(lexer_current(&lexer)->location.column, 6);

	assert_int_equal(lexer_advance(&lexer), LEXER_RESULT_OK);
	assert_int_equal(lexer_current(&lexer)->type, TOKEN_INDENT);
	assert_int_equal(lexer_current(&lexer)->location.line, 2);
	assert_int_equal(lexer_current(&lexer)->location.column, 3);

	assert_int_equal(lexer_advance(&lexer), LEXER_RESULT_OK);
	assert_current_token(&lexer, (expected_token_t){TOKEN_IDENTIFIER, "second"});
	assert_int_equal(lexer_current(&lexer)->location.line, 2);
	assert_int_equal(lexer_current(&lexer)->location.column, 3);

	lexer_destroy(&lexer);
	assert_int_equal(fclose(source), 0);
}

/**
 * @brief Example: token, multiline comment, next line; checks NEWLINE location.
 */
static void test_boundaries_comment_newline_location(void **state) {
	(void)state;

	FILE *source = source_from_text("x/*\n*/\ny\n");
	lexer_t lexer;

	assert_int_equal(lexer_init(&lexer, source), LEXER_RESULT_OK);
	assert_int_equal(lexer_next(&lexer)->type, TOKEN_NEWLINE);
	assert_int_equal(lexer_next(&lexer)->location.line, 1);
	assert_int_equal(lexer_next(&lexer)->location.column, 4);

	lexer_destroy(&lexer);
	assert_int_equal(fclose(source), 0);
}

/**
 * @brief Example: NULL lexer; checks invalid pointer arguments.
 */
static void test_boundaries_reject_null_lexer(void **state) {
	(void)state;

	assert_int_equal(lexer_init(NULL, NULL), LEXER_RESULT_INVALID_ARGUMENT);
	assert_int_equal(lexer_advance(NULL), LEXER_RESULT_INVALID_ARGUMENT);
	assert_int_equal(lexer_scan_token(NULL, NULL), LEXER_RESULT_INVALID_ARGUMENT);
}

/**
 * @brief Example: indent_count equals capacity; checks invalid indentation state.
 */
static void test_boundaries_reject_invalid_indent_count(void **state) {
	(void)state;

	FILE *source = source_from_text("");
	lexer_t lexer;
	token_t token = {0};
	assert_int_equal(lexer_init(&lexer, source), LEXER_RESULT_OK);

	lexer.indent_count = lexer.indentation_capacity;
	assert_int_equal(lexer_scan_token(&lexer, &token), LEXER_RESULT_INVALID_ARGUMENT);
	lexer.indent_count = 0;
	lexer_destroy(&lexer);
	assert_int_equal(fclose(source), 0);
}

/**
 * @brief Example: parenthesis_depth at SIZE_MAX; checks nesting overflow.
 */
static void test_boundaries_reject_parenthesis_overflow(void **state) {
	(void)state;

	FILE *source = source_from_text("");
	lexer_t lexer;
	token_t token = {0};
	assert_int_equal(lexer_init(&lexer, source), LEXER_RESULT_OK);
	lexer.input = '(';
	lexer.at_line_start = false;
	lexer.parenthesis_depth = SIZE_MAX;
	assert_int_equal(lexer_scan_token(&lexer, &token), LEXER_RESULT_MEMORY_ERROR);
	lexer_destroy(&lexer);
	assert_int_equal(fclose(source), 0);
}

/**
 * @brief Example: column at SIZE_MAX; checks token position overflow.
 */
static void test_boundaries_reject_column_overflow(void **state) {
	(void)state;

	FILE *source = source_from_text("");
	lexer_t lexer;
	token_t token = {0};
	assert_int_equal(lexer_init(&lexer, source), LEXER_RESULT_OK);
	lexer.input = 'a';
	lexer.at_line_start = false;
	lexer.column = SIZE_MAX;
	assert_int_equal(lexer_scan_token(&lexer, &token), LEXER_RESULT_MEMORY_ERROR);
	lexer_destroy(&lexer);
	assert_int_equal(fclose(source), 0);
}

/**
 * @brief Example: line at SIZE_MAX; checks line position overflow.
 */
static void test_boundaries_reject_line_overflow(void **state) {
	(void)state;

	FILE *source = source_from_text("");
	lexer_t lexer;
	token_t token = {0};
	assert_int_equal(lexer_init(&lexer, source), LEXER_RESULT_OK);
	lexer.input = '\n';
	lexer.at_line_start = false;
	lexer.line = SIZE_MAX;
	assert_int_equal(lexer_scan_token(&lexer, &token), LEXER_RESULT_MEMORY_ERROR);
	lexer_destroy(&lexer);
	assert_int_equal(fclose(source), 0);
}

/**
 * @brief Example: `$`, backtick; checks unsupported symbols.
 */
static void test_boundaries_reject_unknown_and_non_ascii_input(void **state) {
	(void)state;

	assert_lexical_error("$\n");
	assert_lexical_error("`\n");
}

/**
 * @brief Example: `name` followed by UTF-8; checks non-ASCII identifier bytes.
 */
static void test_boundaries_reject_non_ascii_identifier_byte(void **state) {
	(void)state;

	assert_lexical_error("name\xC3\xA1\n");
}

/**
 * @brief Example: `"á"`; checks non-ASCII bytes are rejected inside string literals.
 */
static void test_boundaries_reject_non_ascii_string_byte(void **state) {
	(void)state;

	assert_lexical_error("\"\xC3\xA1\"\n");
}

/**
 * @brief Example: `/* á *\/`; checks non-ASCII bytes are rejected inside block comments.
 */
static void test_boundaries_reject_non_ascii_block_comment_byte(void **state) {
	(void)state;

	assert_lexical_error("/* \xC3\xA1 */\n");
}

/**
 * @brief Example: `value $ other`; checks unknown symbols are rejected in expressions.
 */
static void test_boundaries_reject_unknown_symbol_in_expression(void **state) {
	(void)state;

	assert_lexical_error("value $ other\n");
}

// =============================================================================
// Tests: complete program, stress, and repeatability
// =============================================================================

/**
 * @brief Example: `complex_program.ifj26`; checks sequential scan reaches EOF.
 */
static void test_integration_complex_program_reaches_eof(void **state) {
	(void)state;

	FILE *source = fopen("src/tests/complex_program.ifj26", "r");
	lexer_t lexer;
	size_t token_count = 0;
	size_t indent_count = 0;
	size_t dedent_count = 0;
	size_t multiline_string_count = 0;

	assert_non_null(source);
	assert_int_equal(lexer_init(&lexer, source), LEXER_RESULT_OK);

	for (;;) {
		const token_t *token = lexer_current(&lexer);

		++token_count;

		if (token->type == TOKEN_INDENT) {
			++indent_count;
		} else if (token->type == TOKEN_DEDENT) {
			++dedent_count;
		} else if (token->type == TOKEN_MULTILINE_STRING) {
			++multiline_string_count;
		} else if (token->type == TOKEN_EOF) {
			break;
		}

		assert_int_equal(lexer_advance(&lexer), LEXER_RESULT_OK);
	}

	assert_true(token_count > 40);
	assert_int_equal(multiline_string_count, 1);
	assert_int_equal(indent_count, dedent_count);

	lexer_destroy(&lexer);
	assert_int_equal(fclose(source), 0);
}

/**
 * @brief Example: 32,768 `a` characters; checks long identifier contents.
 */
static void test_stress_long_identifier(void **state) {
	(void)state;

	const size_t length = 32768;
	char *source_text = malloc(length + 2);

	assert_non_null(source_text);
	memset(source_text, 'a', length);
	source_text[length] = '\n';
	source_text[length + 1] = '\0';

	FILE *source = source_from_text(source_text);
	lexer_t lexer;

	assert_int_equal(lexer_init(&lexer, source), LEXER_RESULT_OK);
	assert_int_equal(lexer_current(&lexer)->type, TOKEN_IDENTIFIER);
	assert_int_equal(lexer_current(&lexer)->length, length);
	assert_memory_equal(lexer_current(&lexer)->lexeme, source_text, length);
	assert_int_equal(lexer_advance(&lexer), LEXER_RESULT_OK);
	assert_int_equal(lexer_current(&lexer)->type, TOKEN_NEWLINE);

	lexer_destroy(&lexer);
	assert_int_equal(fclose(source), 0);
	free(source_text);
}

/**
 * @brief Example: 96 nested indentation levels; checks growth and matching DEDENTs.
 */
static void test_stress_deep_indentation(void **state) {
	(void)state;

	const size_t depth = 96;
	size_t capacity = 1;
	size_t length = 0;
	char *source_text = malloc(capacity);

	assert_non_null(source_text);
	source_text[0] = '\0';

	for (size_t line = 0; line <= depth; ++line) {
		size_t spaces = line * 2;
		size_t needed = length + spaces + 3;

		if (needed > capacity) {
			capacity = needed * 2;
			source_text = realloc(source_text, capacity);
			assert_non_null(source_text);
		}

		// Fill the line with spaces, then an identifier and a newline.
		memset(source_text + length, ' ', spaces);
		length += spaces;
		source_text[length++] = 'x';
		source_text[length++] = '\n';
		source_text[length] = '\0';
	}

	FILE *source = source_from_text(source_text);
	lexer_t lexer;
	size_t indent_count = 0;
	size_t dedent_count = 0;
	size_t identifier_count = 0;

	assert_int_equal(lexer_init(&lexer, source), LEXER_RESULT_OK);

	for (;;) {
		const token_t *token = lexer_current(&lexer);

		if (token->type == TOKEN_INDENT) {
			++indent_count;
		} else if (token->type == TOKEN_DEDENT) {
			++dedent_count;
		} else if (token->type == TOKEN_IDENTIFIER) {
			++identifier_count;
		} else if (token->type == TOKEN_EOF) {
			break;
		}

		assert_int_equal(lexer_advance(&lexer), LEXER_RESULT_OK);
	}

	assert_int_equal(identifier_count, depth + 1);
	assert_int_equal(indent_count, depth);
	assert_int_equal(dedent_count, depth);

	lexer_destroy(&lexer);
	assert_int_equal(fclose(source), 0);
	free(source_text);
}

/**
 * @brief Example: same program scanned 100 times; checks state resets per lexer.
 */
static void test_stress_repeated_full_scans(void **state) {
	(void)state;

	const char *program = "import ifj26.*\n"
	                      "@main def run(): Unit =\n"
	                      "  val number = 1_000\n"
	                      "  print(\"value: \" + toStr(number))\n";

	for (size_t pass = 0; pass < 100; ++pass) {
		FILE *source = source_from_text(program);
		lexer_t lexer;
		size_t token_count = 0;

		assert_int_equal(lexer_init(&lexer, source), LEXER_RESULT_OK);

		for (;;) {
			++token_count;

			if (lexer_current(&lexer)->type == TOKEN_EOF) {
				break;
			}

			assert_int_equal(lexer_advance(&lexer), LEXER_RESULT_OK);
		}

		assert_true(token_count > 20);
		lexer_destroy(&lexer);
		assert_int_equal(fclose(source), 0);
	}
}

/**
 * @brief Runs a named CMocka category and updates main's final status.
 *
 * The CMocka macro receives the original test array, preserving its compiletime element count.
 * A helper function would receive only a pointer and lose that information.
 */
#define RUN_CATEGORY(name, tests)                                                                            \
	do {                                                                                                     \
		printf("\n=== Lexer category: %s ===\n", (name));                                                    \
		status |= cmocka_run_group_tests_name((name), (tests), NULL, NULL);                                  \
	} while (0)

int main(void) {
	const struct CMUnitTest api_tests[] = {
	    cmocka_unit_test(test_api_current_next_and_advance),
	    cmocka_unit_test(test_api_empty_input_contains_eof),
	};
	const struct CMUnitTest identifier_tests[] = {
	    cmocka_unit_test(test_identifiers_all_keywords_literals),
	    cmocka_unit_test(test_identifiers_boundaries_and_case_sensitivity),
	};
	const struct CMUnitTest annotation_tests[] = {
	    cmocka_unit_test(test_identifiers_main_annotation),
	    cmocka_unit_test(test_identifiers_reject_main_annotation_suffix),
	    cmocka_unit_test(test_identifiers_reject_unknown_annotation),
	    cmocka_unit_test(test_identifiers_reject_bare_at_sign),
	    cmocka_unit_test(test_identifiers_reject_incomplete_main_annotation),
	};
	const struct CMUnitTest symbol_tests[] = {
	    cmocka_unit_test(test_symbols_every_supported_terminal),
	    cmocka_unit_test(test_symbols_pair_prefixes_and_repeated_operators),
	};
	const struct CMUnitTest symbol_error_tests[] = {
	    cmocka_unit_test(test_symbols_reject_incomplete_ampersand),
	};
	const struct CMUnitTest valid_number_tests[] = {
	    cmocka_unit_test(test_numbers_valid_forms),
	    cmocka_unit_test(test_numbers_separators_in_each_component),
	    cmocka_unit_test(test_numbers_double_forms_and_boundaries),
	};
	const struct CMUnitTest invalid_number_tests[] = {
	    cmocka_unit_test(test_numbers_reject_leading_zero),
	    cmocka_unit_test(test_numbers_reject_misplaced_separator),
	    cmocka_unit_test(test_numbers_reject_incomplete_exponent),
	    cmocka_unit_test(test_numbers_reject_identifier_suffix),
	    cmocka_unit_test(test_numbers_reject_zero_with_repeated_separators),
	    cmocka_unit_test(test_numbers_reject_exponent_without_digits_after_sign),
	    cmocka_unit_test(test_numbers_reject_fraction_without_digits),
	    cmocka_unit_test(test_numbers_reject_separator_before_exponent),
	};
	const struct CMUnitTest valid_string_tests[] = {
	    cmocka_unit_test(test_strings_normal_escapes),
	    cmocka_unit_test(test_strings_multiline_is_one_raw_token),
	    cmocka_unit_test(test_strings_empty_literals),
	    cmocka_unit_test(test_strings_adjacent_literals),
	    cmocka_unit_test(test_strings_multiline_quotes_and_following_line),
	};
	const struct CMUnitTest invalid_string_tests[] = {
	    cmocka_unit_test(test_strings_reject_invalid_escape),
	    cmocka_unit_test(test_strings_reject_unclosed_ordinary),
	    cmocka_unit_test(test_strings_reject_unclosed_multiline),
	    cmocka_unit_test(test_strings_reject_escape_at_end_of_input),
	    cmocka_unit_test(test_strings_reject_carriage_return),
	};
	const struct CMUnitTest comment_tests[] = {
	    cmocka_unit_test(test_comments_skip_ignored_text),
	    cmocka_unit_test(test_comments_multiline_comment_ends_logical_line),
	    cmocka_unit_test(test_comments_single_line_separate_adjacent_tokens),
	    cmocka_unit_test(test_comments_line_comment_at_eof),
	    cmocka_unit_test(test_comments_nested_multiline_and_comment_only_lines),
	};
	const struct CMUnitTest comment_error_tests[] = {
	    cmocka_unit_test(test_comments_reject_code_after_multiline_comment),
	    cmocka_unit_test(test_comments_reject_unclosed_block_comment),
	    cmocka_unit_test(test_comments_reject_unterminated_nested_comment),
	    cmocka_unit_test(test_comments_reject_non_ascii_comment_byte),
	};
	const struct CMUnitTest indentation_tests[] = {
	    cmocka_unit_test(test_layout_single_indent_and_eof_dedent),
	    cmocka_unit_test(test_layout_multiple_dedents_on_one_line),
	    cmocka_unit_test(test_layout_ignores_whitespace_after_single_line_block_comment),
	    cmocka_unit_test(test_layout_comment_before_code_without_leading_spaces),
	    cmocka_unit_test(test_layout_tab_after_comment_is_separating_whitespace),
	    cmocka_unit_test(test_layout_comment_spacing_matrix),
	    cmocka_unit_test(test_layout_blank_lines_do_not_close_indentation),
	    cmocka_unit_test(test_layout_multiple_dedents_after_comment_line),
	    cmocka_unit_test(test_layout_ignores_tabs_on_blank_lines),
	};
	const struct CMUnitTest continuation_tests[] = {
	    cmocka_unit_test(test_layout_nested_brackets_suppress_physical_newlines),
	    cmocka_unit_test(test_layout_parentheses_suppress_commented_newline),
	    cmocka_unit_test(test_layout_eof_without_final_newline_closes_blocks),
	};
	const struct CMUnitTest layout_error_tests[] = {
	    cmocka_unit_test(test_layout_rejects_leading_tab),
	    cmocka_unit_test(test_layout_rejects_tab_after_leading_spaces),
	    cmocka_unit_test(test_layout_rejects_dedent_to_unseen_width),
	    cmocka_unit_test(test_layout_rejects_unindented_parenthesis_continuation),
	    cmocka_unit_test(test_layout_rejects_tab_before_comment_and_code),
	    cmocka_unit_test(test_layout_rejects_crlf),
	};
	const struct CMUnitTest location_tests[] = {
	    cmocka_unit_test(test_boundaries_token_locations),
	    cmocka_unit_test(test_boundaries_comment_newline_location),
	};
	const struct CMUnitTest defensive_tests[] = {
	    cmocka_unit_test(test_boundaries_reject_null_lexer),
	    cmocka_unit_test(test_boundaries_reject_invalid_indent_count),
	    cmocka_unit_test(test_boundaries_reject_parenthesis_overflow),
	    cmocka_unit_test(test_boundaries_reject_column_overflow),
	    cmocka_unit_test(test_boundaries_reject_line_overflow),
	};
	const struct CMUnitTest invalid_input_tests[] = {
	    cmocka_unit_test(test_boundaries_reject_unknown_and_non_ascii_input),
	    cmocka_unit_test(test_boundaries_reject_non_ascii_identifier_byte),
	    cmocka_unit_test(test_boundaries_reject_non_ascii_string_byte),
	    cmocka_unit_test(test_boundaries_reject_non_ascii_block_comment_byte),
	    cmocka_unit_test(test_boundaries_reject_unknown_symbol_in_expression),
	};
	const struct CMUnitTest integration_tests[] = {
	    cmocka_unit_test(test_integration_complex_program_reaches_eof),
	};
	const struct CMUnitTest stress_tests[] = {
	    cmocka_unit_test(test_stress_long_identifier),
	    cmocka_unit_test(test_stress_deep_indentation),
	    cmocka_unit_test(test_stress_repeated_full_scans),
	};

	int status = 0;

	RUN_CATEGORY("Public API", api_tests);
	RUN_CATEGORY("Identifiers and keywords", identifier_tests);
	RUN_CATEGORY("Main annotation", annotation_tests);
	RUN_CATEGORY("Punctuation and operators", symbol_tests);
	RUN_CATEGORY("Invalid punctuation", symbol_error_tests);
	RUN_CATEGORY("Valid numbers", valid_number_tests);
	RUN_CATEGORY("Invalid numbers", invalid_number_tests);
	RUN_CATEGORY("Valid strings", valid_string_tests);
	RUN_CATEGORY("Invalid strings", invalid_string_tests);
	RUN_CATEGORY("Comments and logical lines", comment_tests);
	RUN_CATEGORY("Invalid comments", comment_error_tests);
	RUN_CATEGORY("Indentation", indentation_tests);
	RUN_CATEGORY("Line continuation", continuation_tests);
	RUN_CATEGORY("Invalid layout", layout_error_tests);
	RUN_CATEGORY("Token locations", location_tests);
	RUN_CATEGORY("Defensive state checks", defensive_tests);
	RUN_CATEGORY("Invalid input bytes", invalid_input_tests);
	RUN_CATEGORY("Complete program", integration_tests);
	RUN_CATEGORY("Stress and repeatability", stress_tests);

	return status == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
