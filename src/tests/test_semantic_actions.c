/**
 * @file test_semantic_actions.c
 * @author xkohela00
 * @brief Tests of the semantic actions
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

#include "../include/lexer.h"
#include "../include/semantic_anal.h"
#include "../include/syntax_anal.h"

// HELPERS

#define MAIN(body) "import ifj26.*\n@main def main(): Unit =\n" body

/** Runs lexer, parser and semantic analysis and returns the exit code the compiler would give. */
static int compile(const char *source) {
	FILE *file = tmpfile();
	if (file == NULL || fputs(source, file) == EOF || fflush(file) != 0 || fseek(file, 0, SEEK_SET) != 0) {
		if (file != NULL)
			fclose(file);
		return 99;
	}

	lexer_t lexer;
	int result = 0;
	lexer_result_t lexer_result = lexer_init(&lexer, file);
	if (lexer_result != LEXER_RESULT_OK) {
		result = lexer_result == LEXER_RESULT_LEXICAL_ERROR ? 1 : 99;
	} else {
		syntax_parser_t parser;
		if (!syntax_parser_init(&parser, &lexer)) {
			result = 99;
		} else if (!syntax_parse_program(&parser)) {
			const lexer_error_t *error = lexer_error(&lexer);
			if (parser.semantic_result != SEM_OK)
				result = parser.semantic_result;
			else if (parser.internal_error)
				result = 99;
			else if (error != NULL)
				result = error->code == LEXER_RESULT_LEXICAL_ERROR ? 1 : 99;
			else
				result = 2;
		}
	}
	lexer_destroy(&lexer);
	fclose(file);
	return result;
}

static int mismatches; // wrong results in the current test, every case is checked before failing

static void check_source(int expected, const char *source, int line) {
	const int got = compile(source);
	sem_actions_free();
	if (got != expected) {
		mismatches++;
		print_error("line %d: expected %d, got %d for:\n----\n%s\n----\n", line, expected, got, source);
	}
}

#define EXPECT(expected, source) check_source((expected), (source), __LINE__)
#define FINISH() assert_int_equal(mismatches, 0)

static int reset(void **state) {
	(void)state;
	mismatches = 0;
	sem_actions_free();
	return 0;
}

static ast_t *taken_ast;
static funct_t *taken_functions;

/** Compiles a valid program and returns the root of its AST, freed by the teardown. */
static ast_node_t *compile_tree(const char *source) {
	assert_int_equal(compile(source), 0);
	assert_true(sem_actions_take(&taken_ast, &taken_functions));
	return ast_root(taken_ast);
}

static int free_tree(void **state) {
	(void)state;
	ast_destroy(&taken_ast);
	if (taken_functions != NULL)
		funct_free(taken_functions);
	taken_functions = NULL;
	sem_actions_free();
	return 0;
}

// body of the first statement of @main, the program is PROGRAM(IMPORT, MAIN(BLOCK(...)))
static ast_node_t *main_statement(ast_node_t *root, size_t index) {
	ast_node_t *main_function = ast_node_child_at(root, 1);
	assert_non_null(main_function);
	assert_int_equal(main_function->kind, AST_NODE_MAIN_FUNCTION);
	ast_node_t *body = ast_node_child_at(main_function, 0);
	assert_int_equal(body->kind, AST_NODE_BLOCK);
	return ast_node_child_at(body, index);
}

// VALID PROGRAMS

static void spec_factorial_example_is_valid(void **state) {
	(void)state;
	EXPECT(0, "import ifj26.*\n"
	          "\n"
	          "def factorialIterative(n: Int): Int =\n"
	          "  var result = 1\n"
	          "  var i: Int = 2\n"
	          "  while i <= n do\n"
	          "    result = result * i\n"
	          "    i = i + 1\n"
	          "  result\n"
	          "\n"
	          "def factorialRecursive(n: Int): Int =\n"
	          "  if n <= 1 then\n"
	          "    return 1\n"
	          "  else\n"
	          "    val previous = n - 1\n"
	          "    val smaller = factorialRecursive(previous)\n"
	          "    return n * smaller\n"
	          "\n"
	          "@main def factorialExample(): Unit =\n"
	          "  val n = readInt()\n"
	          "  if n < 0 then\n"
	          "    print(\"Use an integer from 0 to 12.\\n\")\n"
	          "  else\n"
	          "    if n > 12 then\n"
	          "      print(\"Use an integer from 0 to 12.\\n\")\n"
	          "    else\n"
	          "      val iterative = factorialIterative(n)\n"
	          "      print(iterative)\n");
	FINISH();
}

static void statements_and_expressions_are_valid(void **state) {
	(void)state;
	// expression statement starting with a name, the name is not an assignment target
	EXPECT(0, MAIN("  print(\"hi\")\n"));
	EXPECT(0, MAIN("  val x = 1\n  x + 1\n"));
	EXPECT(0, MAIN("  var x = 1; x = x + 1; print(x)\n"));
	EXPECT(0, MAIN("  val b = !(1 < 2) || true && false\n  print(b)\n"));
	EXPECT(0, MAIN("  val s = \"a\" + \"b\"\n  print(-1 + +2 * 3 / 4)\n"));
	EXPECT(0, MAIN("  val d: Double = 1\n  print(d * 2.5)\n"));
	EXPECT(0, MAIN("  val u = ()\n  print(u)\n"));
	// functions called before their definition, return types inferred from the body
	EXPECT(0, "import ifj26.*\n"
	          "@main def main(): Unit =\n"
	          "  print(twice(add(1, 2)))\n"
	          "def add(a: Int, b: Int) = a + b\n"
	          "def twice(x: Int): Int =\n"
	          "  return x * 2\n");
	EXPECT(0, "import ifj26.*\n"
	          "def nothing(): Unit =\n"
	          "  return\n"
	          "@main def main(): Unit = nothing()\n");
	FINISH();
}

static void control_extension_is_valid(void **state) {
	(void)state;
	EXPECT(0, MAIN("  val x = 3\n  val y = if x < 0 then 0 - x else x\n  print(y)\n"));
	EXPECT(0, MAIN("  val x = 3\n  print(if x > 1 then \"big\" else \"small\")\n"));
	EXPECT(0, MAIN("  val x = 3\n  if x < 0 then print(1) else if x == 0 then print(2) else print(3)\n"));
	EXPECT(0, MAIN("  val x = 3\n  if x < 0 then\n    print(1)\n"));
	EXPECT(0, MAIN("  for i <- 1 to 10 by 2 do print(i)\n"));
	EXPECT(0, MAIN("  val limit = 10\n"
	               "  for\n"
	               "    i <- 0 until limit\n"
	               "    if i != 3\n"
	               "    if i < 8\n"
	               "  do\n"
	               "    print(i)\n"));
	EXPECT(0, MAIN("  for i <- 0 until 3 if i > 0 do\n    val j = i\n    print(j)\n"));
	FINISH();
}

static void valmem_members_are_valid(void **state) {
	(void)state;
	EXPECT(0, MAIN("  val n = \"abc\".length\n  print(n)\n"));
	EXPECT(0, MAIN("  val x = 2.5\n  val i: Int = x.floor.toInt\n  print(i.toString)\n"));
	FINISH();
}

// SEMANTIC ERRORS

static void undefined_names_are_error_3(void **state) {
	(void)state;
	EXPECT(3, MAIN("  print(x)\n"));
	EXPECT(3, MAIN("  x = 1\n"));
	EXPECT(3, MAIN("  foo(1)\n"));
	EXPECT(3, "import ifj26.*\ndef f(): Unit = ()\n");
	FINISH();
}

static void redefinitions_are_error_4(void **state) {
	(void)state;
	EXPECT(4, MAIN("  val x = 1\n  val x = 2\n"));
	EXPECT(4, "import ifj26.*\ndef f(): Unit = ()\ndef f(): Unit = ()\n@main def main(): Unit = ()\n");
	EXPECT(4, "import ifj26.*\ndef print(x: Int): Unit = ()\n@main def main(): Unit = ()\n");
	EXPECT(4, "import ifj26.*\n@main def a(): Unit = ()\n@main def b(): Unit = ()\n");
	FINISH();
}

static void wrong_arguments_are_error_5(void **state) {
	(void)state;
	EXPECT(5, MAIN("  print(1, 2)\n"));
	EXPECT(5, MAIN("  val n = strLength(1)\n"));
	EXPECT(5, "import ifj26.*\ndef f(a: Int): Int = a\n@main def main(): Unit =\n  f(\"x\")\n");
	EXPECT(5, "import ifj26.*\n@main def main(a: Int): Unit = ()\n");
	FINISH();
}

static void type_errors_are_error_6(void **state) {
	(void)state;
	EXPECT(6, MAIN("  val x: Int = \"a\"\n"));
	EXPECT(6, MAIN("  val x = 1 + \"a\"\n"));
	EXPECT(6, MAIN("  if 1 then print(1)\n"));
	EXPECT(6, MAIN("  while 1 do print(1)\n"));
	EXPECT(6, MAIN("  for i <- 1.5 to 3 do print(i)\n"));
	EXPECT(6, MAIN("  val n = (5).length\n"));
	EXPECT(6, MAIN("  val n = \"abc\".size\n"));
	EXPECT(6, "import ifj26.*\ndef f(): Int =\n  \"a\"\n@main def main(): Unit = ()\n");
	EXPECT(6, "import ifj26.*\ndef f(n: Int) = f(n)\n@main def main(): Unit = ()\n");
	FINISH();
}

static void writes_to_immutable_are_error_7(void **state) {
	(void)state;
	EXPECT(7, MAIN("  val x = 1\n  x = 2\n"));
	EXPECT(7, "import ifj26.*\ndef f(a: Int): Unit =\n  a = 2\n@main def main(): Unit = ()\n");
	EXPECT(7, MAIN("  for i <- 1 to 3 do i = 5\n"));
	FINISH();
}

static void zero_step_is_error_10(void **state) {
	(void)state;
	EXPECT(10, MAIN("  for i <- 1 to 3 by 0 do print(i)\n"));
	EXPECT(10, MAIN("  for i <- 1 to 3 by -0 do print(i)\n"));
	FINISH();
}

static void only_names_can_be_called(void **state) {
	(void)state;
	EXPECT(2, MAIN("  val x = print(1)(2)\n"));
	EXPECT(2, MAIN("  val x = \"abc\".length(1)\n"));
	// a name in parentheses is still a name
	EXPECT(0, MAIN("  (print)(1)\n"));
	FINISH();
}

static void syntax_error_wins_over_semantic_error(void **state) {
	(void)state;
	// the semantic error comes first in the source, but the program is checked only after parsing
	EXPECT(2, MAIN("  print(x)\n  val = 1\n"));
	EXPECT(2, MAIN("  val x = print(1)(2)\n  val = 1\n"));
	FINISH();
}

static void if_value_without_else_is_error_2(void **state) {
	(void)state;
	// spec 5.3: an if used as a value must have an else branch
	EXPECT(2, MAIN("  val x = 1\n  val y = if x < 0 then 1\n"));
	EXPECT(2, MAIN("  val x = 1\n  val y = if x < 0 then 1 else if x > 0 then 2\n"));
	EXPECT(0, MAIN("  val x = 1\n  val y = if x < 0 then 1 else 2\n"));
	FINISH();
}

// SHAPE OF THE AST

static void literals_get_their_values(void **state) {
	(void)state;
	ast_node_t *root = compile_tree(MAIN("  val a = 1_000\n"
	                                     "  val b = 1.5e2d\n"
	                                     "  val c = \"x\\n\\\"y\\\\\"\n"
	                                     "  val d = \"\"\"raw\\n\"\"\"\n"
	                                     "  val e = true\n"));

	ast_node_t *a = main_statement(root, 0);
	assert_int_equal(a->kind, AST_NODE_VAL_DECLARATION);
	assert_string_equal(a->text, "a");
	assert_int_equal(a->first_child->kind, AST_NODE_INTEGER);
	assert_int_equal(a->first_child->value.integer, 1000);

	ast_node_t *b = main_statement(root, 1)->first_child;
	assert_int_equal(b->kind, AST_NODE_DOUBLE);
	assert_true(b->value.decimal == 150.0);

	assert_string_equal(main_statement(root, 2)->first_child->text, "x\n\"y\\");
	assert_string_equal(main_statement(root, 3)->first_child->text, "raw\\n");

	ast_node_t *e = main_statement(root, 4)->first_child;
	assert_int_equal(e->kind, AST_NODE_BOOLEAN);
	assert_true(e->value.boolean);
}

static void expressions_keep_operator_order(void **state) {
	(void)state;
	// 1 - 2 * f(3, x) with x a var, the call gets the name and its arguments in order
	ast_node_t *root = compile_tree("import ifj26.*\n"
	                                "def f(a: Int, b: Int): Int = a\n"
	                                "@main def main(): Unit =\n"
	                                "  var x: Int = 1\n"
	                                "  x = 1 - 2 * f(3, x)\n");

	ast_node_t *function = ast_node_child_at(root, 1);
	assert_int_equal(function->kind, AST_NODE_FUNCTION);
	assert_string_equal(function->text, "f");
	assert_int_equal(function->data_type, SYMT_TYPE_INTEGER);
	assert_int_equal(ast_node_child_count(function), 3); // two parameters and the body
	assert_int_equal(ast_node_child_at(function, 1)->kind, AST_NODE_PARAMETER);
	assert_string_equal(ast_node_child_at(function, 1)->text, "b");
	assert_int_equal(ast_node_child_at(function, 1)->data_type, SYMT_TYPE_INTEGER);

	ast_node_t *root_main = ast_node_child_at(root, 2);
	ast_node_t *declaration = ast_node_child_at(root_main->first_child, 0);
	assert_int_equal(declaration->kind, AST_NODE_VAR_DECLARATION);
	assert_int_equal(declaration->data_type, SYMT_TYPE_INTEGER);

	ast_node_t *assignment = ast_node_child_at(root_main->first_child, 1);
	assert_int_equal(assignment->kind, AST_NODE_ASSIGNMENT);
	assert_string_equal(assignment->text, "x");

	ast_node_t *minus = assignment->first_child;
	assert_int_equal(minus->kind, AST_NODE_SUBTRACT);
	assert_int_equal(minus->first_child->kind, AST_NODE_INTEGER);
	ast_node_t *times = ast_node_child_at(minus, 1);
	assert_int_equal(times->kind, AST_NODE_MULTIPLY);
	ast_node_t *call = ast_node_child_at(times, 1);
	assert_int_equal(call->kind, AST_NODE_CALL);
	assert_string_equal(call->text, "f");
	assert_int_equal(ast_node_child_count(call), 2);
	assert_int_equal(ast_node_child_at(call, 0)->value.integer, 3);
	assert_string_equal(ast_node_child_at(call, 1)->text, "x");
}

static void if_and_for_have_all_parts(void **state) {
	(void)state;
	ast_node_t *root = compile_tree(MAIN("  val x = 1\n"
	                                     "  if x < 0 then print(1) else if x == 0 then print(2)\n"
	                                     "  for i <- 0 to 9 by 3 if i > 1 if i < 8 do\n"
	                                     "    print(i)\n"
	                                     "  return\n"));

	ast_node_t *outer_if = main_statement(root, 1);
	assert_int_equal(outer_if->kind, AST_NODE_IF);
	assert_int_equal(ast_node_child_count(outer_if), 3);
	ast_node_t *inner_if = ast_node_child_at(outer_if, 2);
	assert_int_equal(inner_if->kind, AST_NODE_IF);
	assert_int_equal(ast_node_child_count(inner_if), 2);

	ast_node_t *loop = main_statement(root, 2);
	assert_int_equal(loop->kind, AST_NODE_FOR);
	assert_int_equal(ast_node_child_count(loop), 4); // generator, two guards and the body
	ast_node_t *binding = loop->first_child;
	assert_int_equal(binding->kind, AST_NODE_FOR_BINDING);
	assert_string_equal(binding->text, "i");
	ast_node_t *range = binding->first_child;
	assert_int_equal(range->kind, AST_NODE_RANGE_TO);
	assert_int_equal(ast_node_child_count(range), 3);
	assert_int_equal(ast_node_child_at(range, 2)->value.integer, 3);
	assert_int_equal(ast_node_child_at(loop, 3)->kind, AST_NODE_BLOCK);

	ast_node_t *return_statement = main_statement(root, 3);
	assert_int_equal(return_statement->kind, AST_NODE_RETURN);
	assert_int_equal(ast_node_child_count(return_statement), 0);
}

static void members_become_calls(void **state) {
	(void)state;
	ast_node_t *root = compile_tree(MAIN("  val n = \"abc\".length.toString\n"));
	ast_node_t *outer = main_statement(root, 0)->first_child;
	assert_int_equal(outer->kind, AST_NODE_CALL);
	assert_string_equal(outer->text, ".toString");
	ast_node_t *inner = outer->first_child;
	assert_string_equal(inner->text, ".length");
	assert_int_equal(inner->first_child->kind, AST_NODE_STRING);
}

int main(void) {
	const struct CMUnitTest tests[] = {
	    cmocka_unit_test_setup(spec_factorial_example_is_valid, reset),
	    cmocka_unit_test_setup(statements_and_expressions_are_valid, reset),
	    cmocka_unit_test_setup(control_extension_is_valid, reset),
	    cmocka_unit_test_setup(valmem_members_are_valid, reset),
	    cmocka_unit_test_setup(undefined_names_are_error_3, reset),
	    cmocka_unit_test_setup(redefinitions_are_error_4, reset),
	    cmocka_unit_test_setup(wrong_arguments_are_error_5, reset),
	    cmocka_unit_test_setup(type_errors_are_error_6, reset),
	    cmocka_unit_test_setup(writes_to_immutable_are_error_7, reset),
	    cmocka_unit_test_setup(zero_step_is_error_10, reset),
	    cmocka_unit_test_setup(only_names_can_be_called, reset),
	    cmocka_unit_test_setup(syntax_error_wins_over_semantic_error, reset),
	    cmocka_unit_test_setup(if_value_without_else_is_error_2, reset),
	    cmocka_unit_test_teardown(literals_get_their_values, free_tree),
	    cmocka_unit_test_teardown(expressions_keep_operator_order, free_tree),
	    cmocka_unit_test_teardown(if_and_for_have_all_parts, free_tree),
	    cmocka_unit_test_teardown(members_become_calls, free_tree),
	};
	return cmocka_run_group_tests_name("SEMANTIC ACTIONS", tests, NULL, NULL);
}
