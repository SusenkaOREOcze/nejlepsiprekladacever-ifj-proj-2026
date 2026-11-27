/**
 * @file test_lexer_symsem_irgen_integration.c
 * @author xkralil00 (SusenkaOREOcze)
 * @date 2026-11-27
 * @brief Source-level integration tests through semantic analysis and TAC.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "cmocka/cmocka.h"

#include "../include/ast.h"
#include "../include/functable.h"
#include "../include/irgen.h"
#include "../include/lexer.h"
#include "../include/semantic_anal.h"
#include "../include/syntax_anal.h"
#include "../include/tac.h"

#define PROLOG "import ifj26.*\n"
#define MAIN(body) PROLOG "@main def main(): Unit =\n" body
#define WITH(definitions, body) PROLOG definitions "@main def main(): Unit =\n" body

/**
 * @brief Parses a source program and optionally generates its TAC.
 * @param source Complete IFJ26 program.
 * @param out_code Receives TAC on success; NULL checks only lexer, parser and semantics.
 * @return Compiler exit code from section 6 of the specification.
 */
static int compile_source(const char *source, tac_t **out_code) {
	sem_actions_free();
	FILE *input = tmpfile();
	if (input == NULL)
		return 99;
	if (fputs(source, input) == EOF || fseek(input, 0, SEEK_SET) != 0) {
		fclose(input);
		return 99;
	}

	lexer_t lexer;
	lexer_result_t lex_result = lexer_init(&lexer, input);
	int status = 0;
	if (lex_result != LEXER_RESULT_OK) {
		status = lex_result == LEXER_RESULT_LEXICAL_ERROR ? 1 : 99;
		goto done;
	}

	syntax_parser_t parser;
	if (!syntax_parser_init(&parser, &lexer)) {
		status = 99;
		goto done;
	}
	if (!syntax_parse_program(&parser)) {
		const lexer_error_t *error = lexer_error(&lexer);
		if (parser.semantic_result != SEM_OK)
			status = parser.semantic_result;
		else if (parser.internal_error)
			status = 99;
		else if (error != NULL)
			status = error->code == LEXER_RESULT_LEXICAL_ERROR ? 1 : 99;
		else
			status = 2;
		goto done;
	}

	status = sem_actions_result();
	if (status != 0 || out_code == NULL)
		goto done;

	ast_t *tree = NULL;
	funct_t *functions = NULL;
	if (!sem_actions_take(&tree, &functions)) {
		status = 99;
		goto done;
	}
	irgen_result_t ir_result = irgen_generate(tree, out_code);
	if (ir_result != IRGEN_OK) {
		fprintf(stderr, "IR generation failed with result %d.\n", ir_result);
		status = 99;
	}
	ast_destroy(&tree);
	funct_free(functions);

done:
	lexer_destroy(&lexer);
	fclose(input);
	sem_actions_free();
	return status;
}

/**
 * @brief Counts one opcode in the generated instruction list.
 */
static size_t count_opcode(const tac_t *code, tac_opcode_t opcode) {
	size_t count = 0;
	for (const tac_instruction_t *it = tac_first(code); it != NULL; it = it->next)
		count += it->opcode == opcode;
	return count;
}

/**
 * @brief Finds an instruction with the given opcode and function name.
 */
static bool has_named_opcode(const tac_t *code, tac_opcode_t opcode, const char *name) {
	for (const tac_instruction_t *it = tac_first(code); it != NULL; it = it->next)
		if (it->opcode == opcode && it->name != NULL && strcmp(it->name, name) == 0)
			return true;
	return false;
}

/**
 * @brief Valid prolog, declaration and call pass through all front-end stages.
 */
static void test_frontend_valid_program(void **state) {
	(void)state;
	assert_int_equal(compile_source(MAIN("  val x: Int = 42\n  print(x)\n"), NULL), 0);
}

/**
 * @brief A tab-only blank line does not change indentation.
 */
static void test_frontend_blank_tab(void **state) {
	(void)state;
	assert_int_equal(compile_source(MAIN("  val x = 1\n\t\n  print(x)\n"), NULL), 0);
}

/**
 * @brief CRLF contains an unsupported carriage return and produces lexical code 1.
 */
static void test_frontend_crlf_error_1(void **state) {
	(void)state;
	assert_int_equal(compile_source("import ifj26.*\r\n@main def main(): Unit = ()\r\n", NULL), 1);
}

/**
 * @brief A block comment before a statement preserves its indentation.
 */
static void test_frontend_comment_before_statement(void **state) {
	(void)state;
	assert_int_equal(compile_source(MAIN("  /* baf */ val y: Int = 15\n  print(y)\n"), NULL), 0);
}

/**
 * @brief An unfinished string is a lexical error, even after a valid statement.
 */
static void test_frontend_lexical_error_1(void **state) {
	(void)state;
	assert_int_equal(compile_source(MAIN("  val x = 1\n  val s = \"unfinished\n"), NULL), 1);
}

/**
 * @brief A missing prolog produces syntax error 2.
 */
static void test_frontend_missing_prolog_error_2(void **state) {
	(void)state;
	assert_int_equal(compile_source("@main def main(): Unit = ()\n", NULL), 2);
}

/**
 * @brief An incomplete if produces syntax error 2.
 */
static void test_frontend_incomplete_if_error_2(void **state) {
	(void)state;
	assert_int_equal(compile_source(MAIN("  if true then\n"), NULL), 2);
}

/**
 * @brief An undefined name produces semantic error 3.
 */
static void test_frontend_undefined_name_error_3(void **state) {
	(void)state;
	assert_int_equal(compile_source(MAIN("  print(unknown)\n"), NULL), 3);
}

/**
 * @brief A missing @main produces semantic error 3.
 */
static void test_frontend_missing_main_error_3(void **state) {
	(void)state;
	assert_int_equal(compile_source(PROLOG "def f(): Unit = ()\n", NULL), 3);
}

/**
 * @brief A duplicate declaration in the same scope produces code 4.
 */
static void test_frontend_redefinition_error_4(void **state) {
	(void)state;
	assert_int_equal(compile_source(MAIN("  val x = 1\n  val x = 2\n"), NULL), 4);
}

/**
 * @brief A wrong argument count produces semantic error 5.
 */
static void test_frontend_argument_count_error_5(void **state) {
	(void)state;
	assert_int_equal(compile_source(WITH("def f(a: Int): Int = a\n", "  f()\n"), NULL), 5);
}

/**
 * @brief A wrong argument type produces semantic error 5.
 */
static void test_frontend_argument_type_error_5(void **state) {
	(void)state;
	assert_int_equal(compile_source(WITH("def f(a: Int): Int = a\n", "  f(\"x\")\n"), NULL), 5);
}

/**
 * @brief An invalid initializer produces semantic error 6.
 */
static void test_frontend_initializer_type_error_6(void **state) {
	(void)state;
	assert_int_equal(compile_source(MAIN("  val x: Int = \"x\"\n"), NULL), 6);
}

/**
 * @brief Incompatible if branches produce semantic error 6.
 */
static void test_frontend_if_branch_type_error_6(void **state) {
	(void)state;
	assert_int_equal(compile_source(MAIN("  val x = if true then 1 else \"x\"\n"), NULL), 6);
}

/**
 * @brief Assigning to val produces semantic error 7.
 */
static void test_frontend_val_assignment_error_7(void **state) {
	(void)state;
	assert_int_equal(compile_source(MAIN("  val x = 1\n  x = 2\n"), NULL), 7);
}

/**
 * @brief Assigning to a parameter produces semantic error 7.
 */
static void test_frontend_parameter_assignment_error_7(void **state) {
	(void)state;
	assert_int_equal(compile_source(WITH("def f(a: Int): Unit =\n  a = 2\n", "  f(1)\n"), NULL), 7);
}

/**
 * @brief A literal zero range step produces semantic code 10.
 */
static void test_frontend_other_semantic_error_10(void **state) {
	(void)state;
	assert_int_equal(compile_source(MAIN("  for i <- 1 to 3 by 0 do print(i)\n"), NULL), 10);
}

/**
 * @brief A later syntax error is reported before semantic analysis of an earlier name.
 */
static void test_frontend_error_precedence(void **state) {
	(void)state;
	assert_int_equal(compile_source(MAIN("  print(unknown)\n  val = 1\n"), NULL), 2);
}

/**
 * @brief A declaration and assignment retain the same symbol ID in TAC.
 */
static void test_tac_assignment_symbol_identity(void **state) {
	(void)state;
	tac_t *code = NULL;
	assert_int_equal(compile_source(MAIN("  var x = 1\n  x = 2\n"), &code), 0);
	assert_non_null(code);
	uint64_t first_id = 0;
	bool found = false;
	bool reassigned = false;
	for (const tac_instruction_t *it = tac_first(code); it != NULL; it = it->next) {
		if (it->opcode != TAC_MOVE || it->dst.kind != TAC_OPERAND_SYMBOL)
			continue;
		if (!found) {
			first_id = it->dst.value.id;
			found = true;
		} else if (it->dst.value.id == first_id) {
			reassigned = true;
		}
	}
	assert_true(found);
	assert_true(reassigned);
	tac_destroy(&code);
}

/**
 * @brief Inline function bodies generate parameters, arithmetic, a call and returns.
 */
static void test_tac_inline_function_and_call(void **state) {
	(void)state;
	tac_t *code = NULL;
	assert_int_equal(compile_source(WITH("def f(a: Int): Int = a + 1\n", "  print(f(2))\n"), &code), 0);
	assert_non_null(code);
	assert_true(has_named_opcode(code, TAC_FUNCTION_BEGIN, "f"));
	assert_true(has_named_opcode(code, TAC_CALL, "f"));
	assert_true(count_opcode(code, TAC_PARAM) >= 1);
	assert_true(count_opcode(code, TAC_ADD) >= 1);
	assert_true(count_opcode(code, TAC_ARG) >= 1);
	assert_true(count_opcode(code, TAC_RETURN) >= 1);
	tac_destroy(&code);
}

/**
 * @brief An if expression with a returning branch still emits a usable alternative.
 */
static void test_tac_if_returning_branch(void **state) {
	(void)state;
	tac_t *code = NULL;
	assert_int_equal(compile_source(WITH("def f(x: Int): Int =\n"
	                                     "  if x > 0 then\n    return 1\n  else\n    2\n",
	                                     "  print(f(1))\n"),
	                                &code),
	                 0);
	assert_non_null(code);
	assert_true(count_opcode(code, TAC_JUMP_IF_FALSE) >= 1);
	assert_true(count_opcode(code, TAC_LABEL) >= 1);
	assert_true(count_opcode(code, TAC_RETURN) >= 2);
	tac_destroy(&code);
}

/**
 * @brief A runtime range step emits CHECK_NONZERO after successful static analysis.
 */
static void test_tac_dynamic_range_step(void **state) {
	(void)state;
	tac_t *code = NULL;
	assert_int_equal(compile_source(MAIN("  val step = 1\n  for i <- 1 to 3 by step do print(i)\n"), &code),
	                 0);
	assert_non_null(code);
	assert_int_equal(count_opcode(code, TAC_CHECK_NONZERO), 1);
	assert_true(count_opcode(code, TAC_JUMP_IF_FALSE) >= 1);
	tac_destroy(&code);
}

/**
 * @brief A Unit-valued if emits both calls without a result temporary.
 */
static void test_tac_unit_if(void **state) {
	(void)state;
	tac_t *code = NULL;
	assert_int_equal(compile_source(WITH("def f(x: Int): Unit =\n"
	                                     "  if x > 0 then\n    print(x)\n  else\n    ()\n",
	                                     "  f(1)\n"),
	                                &code),
	                 0);
	assert_non_null(code);
	assert_true(count_opcode(code, TAC_JUMP_IF_FALSE) >= 1);
	assert_true(has_named_opcode(code, TAC_CALL, "print"));
	tac_destroy(&code);
}

/**
 * @brief A failed program leaves no semantic state in the next compilation.
 */
static void test_pipeline_repeated_after_error(void **state) {
	(void)state;
	tac_t *code = NULL;
	assert_int_equal(compile_source(MAIN("  val x: Int = \"bad\"\n"), &code), 6);
	assert_null(code);
	assert_int_equal(compile_source(MAIN("  val x: Int = 1\n"), &code), 0);
	assert_non_null(code);
	tac_destroy(&code);
}

int main(void) {
	const struct CMUnitTest valid_frontend[] = {
	    cmocka_unit_test(test_frontend_valid_program),
	    cmocka_unit_test(test_frontend_blank_tab),
	    cmocka_unit_test(test_frontend_comment_before_statement),
	};
	const struct CMUnitTest frontend_errors[] = {
	    cmocka_unit_test(test_frontend_lexical_error_1),
	    cmocka_unit_test(test_frontend_crlf_error_1),
	    cmocka_unit_test(test_frontend_missing_prolog_error_2),
	    cmocka_unit_test(test_frontend_incomplete_if_error_2),
	    cmocka_unit_test(test_frontend_undefined_name_error_3),
	    cmocka_unit_test(test_frontend_missing_main_error_3),
	    cmocka_unit_test(test_frontend_redefinition_error_4),
	    cmocka_unit_test(test_frontend_argument_count_error_5),
	    cmocka_unit_test(test_frontend_argument_type_error_5),
	    cmocka_unit_test(test_frontend_initializer_type_error_6),
	    cmocka_unit_test(test_frontend_if_branch_type_error_6),
	    cmocka_unit_test(test_frontend_val_assignment_error_7),
	    cmocka_unit_test(test_frontend_parameter_assignment_error_7),
	    cmocka_unit_test(test_frontend_other_semantic_error_10),
	    cmocka_unit_test(test_frontend_error_precedence),
	};
	const struct CMUnitTest generated_tac[] = {
	    cmocka_unit_test(test_tac_assignment_symbol_identity),
	    cmocka_unit_test(test_tac_inline_function_and_call),
	    cmocka_unit_test(test_tac_if_returning_branch),
	    cmocka_unit_test(test_tac_dynamic_range_step),
	    cmocka_unit_test(test_tac_unit_if),
	    cmocka_unit_test(test_pipeline_repeated_after_error),
	};

	int failed = 0;
	failed += cmocka_run_group_tests_name("LEXER + SYNTAX + SEMANTICS: VALID", valid_frontend, NULL, NULL);
	failed += cmocka_run_group_tests_name("LEXER + SYNTAX + SEMANTICS: ERRORS", frontend_errors, NULL, NULL);
	failed += cmocka_run_group_tests_name("LEXER THROUGH IRGEN: TAC", generated_tac, NULL, NULL);
	sem_actions_free();
	return failed;
}
