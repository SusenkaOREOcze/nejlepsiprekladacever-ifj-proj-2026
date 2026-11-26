/**
   @author xkohela00

   Every test feeds IFJ26 source text through lexer and parser.

   Expected results use the exit codes:
   0 = program is syntactically correct (semantic errors 3 to 7 still count as 0 here,
       because the parser alone can't detect them),
   1 = lexical error,
   2 = syntax error.
*/

#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/lexer.h"
#include "../include/semantic_anal.h"
#include "../include/syntax_anal.h"

#include "cmocka/cmocka.h"

/*
  HELPERS
*/

enum {
	RESULT_OK = 0,
	RESULT_LEXICAL = 1,
	RESULT_SYNTAX = 2,
	RESULT_INTERNAL = 99,
};

#define MAIN(body) "import ifj26.*\n@main def main(): Unit =\n" body

/** Runs the lexer and the parser on the source and returns the exit code the compiler would give. */
static int analyse(const char *source) {
	FILE *file = tmpfile();
	if (file == NULL || fputs(source, file) == EOF || fflush(file) != 0 || fseek(file, 0, SEEK_SET) != 0) {
		if (file != NULL) {
			fclose(file);
		}
		return RESULT_INTERNAL;
	}

	lexer_t lexer;
	int result = RESULT_OK;
	lexer_result_t lexer_result = lexer_init(&lexer, file);
	if (lexer_result != LEXER_RESULT_OK) {
		result = lexer_result == LEXER_RESULT_LEXICAL_ERROR ? RESULT_LEXICAL : RESULT_INTERNAL;
	} else {
		syntax_parser_t parser;
		if (!syntax_parser_init(&parser, &lexer)) {
			result = RESULT_INTERNAL;
		} else if (!syntax_parse_program(&parser)) {
			const lexer_error_t *error = lexer_error(&lexer);
			if (parser.internal_error || parser.semantic_result == SEM_ERR_INTERNAL) {
				result = RESULT_INTERNAL;
			} else if (parser.semantic_result != SEM_OK) {
				/*
				 * the whole program was parsed, semantic errors are tested in test_semantic_actions.c,
				 * only a call of something else than a name is a syntax error found by the semantic analyser
				 */
				result = parser.semantic_result == SEM_ERR_SYNTAX ? RESULT_SYNTAX : RESULT_OK;
			} else if (error != NULL) {
				result = error->code == LEXER_RESULT_LEXICAL_ERROR ? RESULT_LEXICAL : RESULT_INTERNAL;
			} else {
				result = RESULT_SYNTAX;
			}
		}
	}
	sem_actions_free();
	lexer_destroy(&lexer);
	fclose(file);
	return result;
}

static int mismatches; // wrong results in the current test, every case gets checked before failing

static void check_source(int expected, const char *source, int line) {
	int got = analyse(source);
	if (got != expected) {
		mismatches++;
		print_error("line %d: expected %d, got %d for:\n----\n%s\n----\n", line, expected, got, source);
	}
}

#define EXPECT(expected, source) check_source((expected), (source), __LINE__)
#define ACCEPTED(source) EXPECT(RESULT_OK, source)
#define LEXICAL_ERROR(source) EXPECT(RESULT_LEXICAL, source)
#define SYNTAX_ERROR(source) EXPECT(RESULT_SYNTAX, source)

static int reset_mismatches(void **state) {
	(void)state;
	mismatches = 0;
	return 0;
}

// each test checks all of its cases first, then fails here if any of them was wrong
#define FINISH() assert_int_equal(mismatches, 0)

#define SPEC_TEST(name) cmocka_unit_test_setup(name, reset_mismatches)

#define EXEC_TEST_CATEGORY(name, tests)                                                                      \
	do {                                                                                                     \
		printf("\n[    " name "    ]\n");                                                                    \
		test_results |= cmocka_run_group_tests_name((name), (tests), NULL, NULL);                            \
	} while (0)

/*===============
  PROGRAM STRUCTURE, SECTION 2
  ===============*/

static void spec_factorial_example(void **state) {
	(void)state;
	// the example program from section 2
	ACCEPTED("import ifj26.*\n"
	         "def factorial(n: Int): Int =\n"
	         "  var result = 1\n"
	         "  var i: Int = n // uvedeni typu je nepovinne\n"
	         "  while i > 1 do\n"
	         "    result = result * i\n"
	         "    i = i - 1\n"
	         "  result // posledni vyraz v tele funkce urci navratovou hodnotu\n"
	         "@main def runFoo(): Unit =\n"
	         "  val n = readInt()\n"
	         "  val result = factorial(n)\n"
	         "  print(result)\n"
	         "  print(\"\\n\")\n");
	FINISH();
}

static void prolog_is_required_once(void **state) {
	(void)state;
	// absence of the prolog is syntax error 2
	SYNTAX_ERROR("@main def main(): Unit =\n"
	             "  print(1)\n");
	// prolog has to be there exactly once
	SYNTAX_ERROR("import ifj26.*\n"
	             "import ifj26.*\n"
	             "@main def main(): Unit =\n"
	             "  print(1)\n");
	// prolog has to be the first thing in the file
	SYNTAX_ERROR("@main def main(): Unit =\n"
	             "  print(1)\n"
	             "import ifj26.*\n");
	FINISH();
}

static void prolog_has_exact_form(void **state) {
	(void)state;
	SYNTAX_ERROR("import ifj25.*\n"
	             "@main def main(): Unit =\n"
	             "  print(1)\n");
	SYNTAX_ERROR("import ifj26\n"
	             "@main def main(): Unit =\n"
	             "  print(1)\n");
	SYNTAX_ERROR("import ifj26.print\n"
	             "@main def main(): Unit =\n"
	             "  print(1)\n");
	SYNTAX_ERROR("import ifj26.* @main def main(): Unit =\n"
	             "  print(1)\n");
	// any whitespace between the tokens is fine
	ACCEPTED("import   ifj26 . *\n"
	         "@main def main(): Unit =\n"
	         "  print(1)\n");
	FINISH();
}

static void main_annotation_placement(void **state) {
	(void)state;
	// annotation can be on its own line or on the same line as def
	ACCEPTED("import ifj26.*\n"
	         "@main\n"
	         "def start(): Unit =\n"
	         "  print(1)\n");
	ACCEPTED("import ifj26.*\n"
	         "@main def start(): Unit =\n"
	         "  print(1)\n");
	// nothing can be between @ and main
	LEXICAL_ERROR("import ifj26.*\n"
	              "@ main def start(): Unit =\n"
	              "  print(1)\n");
	FINISH();
}

static void main_rules_are_semantic(void **state) {
	(void)state;
	// missing main is error 3, not a syntax error
	ACCEPTED("import ifj26.*\n"
	         "def f(): Unit =\n"
	         "  print(1)\n");
	// two mains is error 4
	ACCEPTED("import ifj26.*\n"
	         "@main def a(): Unit =\n"
	         "  print(1)\n"
	         "@main def b(): Unit =\n"
	         "  print(2)\n");
	// a parameter is error 5, other return type error 6
	ACCEPTED("import ifj26.*\n"
	         "@main def main(x: Int): Int =\n"
	         "  return x\n");
	FINISH();
}

static void only_functions_at_top_level(void **state) {
	(void)state;
	SYNTAX_ERROR("import ifj26.*\n"
	             "val x = 1\n"
	             "@main def main(): Unit =\n"
	             "  print(x)\n");
	SYNTAX_ERROR("import ifj26.*\n"
	             "print(1)\n");
	SYNTAX_ERROR("import ifj26.*\n"
	             "@main def main(): Unit =\n"
	             "  print(1)\n"
	             "print(2)\n");
	FINISH();
}

/*
  FUNCTIONS, SECTION 2.3
*/

static void function_forms(void **state) {
	(void)state;
	// one line form
	ACCEPTED("import ifj26.*\n"
	         "def add(a: Int, b: Int): Int = a + b\n"
	         "@main def main(): Unit =\n"
	         "  print(add(1, 2))\n");
	// return type left out
	ACCEPTED("import ifj26.*\n"
	         "def one() = 1\n"
	         "def two() =\n"
	         "  val x = 2\n"
	         "  x\n"
	         "@main def main(): Unit =\n"
	         "  print(one())\n");
	// every parameter type
	ACCEPTED("import ifj26.*\n"
	         "def f(a: Int, b: Double, c: String, d: Unit, e: Boolean): Unit =\n"
	         "  print(a)\n"
	         "@main def main(): Unit =\n"
	         "  print(1)\n");
	// function called before its definition
	ACCEPTED("import ifj26.*\n"
	         "@main def main(): Unit =\n"
	         "  later()\n"
	         "def later(): Unit =\n"
	         "  print(1)\n");
	FINISH();
}

static void function_header_errors(void **state) {
	(void)state;
	// every parameter has the form id : type
	SYNTAX_ERROR("import ifj26.*\n"
	             "def f(a): Int = a\n");
	SYNTAX_ERROR("import ifj26.*\n"
	             "def f(a Int): Int = a\n");
	SYNTAX_ERROR("import ifj26.*\n"
	             "def f(a: Int b: Int): Int = a\n");
	SYNTAX_ERROR("import ifj26.*\n"
	             "def f: Int = 1\n");
	SYNTAX_ERROR("import ifj26.*\n"
	             "def (): Int = 1\n");
	SYNTAX_ERROR("import ifj26.*\n"
	             "def f(): Int 1\n");
	SYNTAX_ERROR("import ifj26.*\n"
	             "def f(): = 1\n");
	// keyword can't be a function name
	SYNTAX_ERROR("import ifj26.*\n"
	             "def while(): Int = 1\n");
	FINISH();
}

/*
  LINES, INDENTATION, SEPARATORS
*/

static void semicolon_separates_statements(void **state) {
	(void)state;
	ACCEPTED(MAIN("  var x = 1; x = 2; print(x)\n"));
	// two statements on one line need a semicolon
	SYNTAX_ERROR(MAIN("  var x = 1 x = 2\n"));
	SYNTAX_ERROR(MAIN("  print(1) print(2)\n"));
	FINISH();
}

static void no_separator_needed_at_the_end(void **state) {
	(void)state;
	// last statement before end of file doesnt need a newline
	ACCEPTED(MAIN("  print(1)"));
	ACCEPTED("import ifj26.*\n"
	         "def f(): Int = 1");
	FINISH();
}

static void newlines_inside_parentheses_are_whitespace(void **state) {
	(void)state;
	/* every line inside parentheses, the one with the closing paren too, has to be indented
	   more than the start of its logical line */
	ACCEPTED(MAIN("  val x = (1 +\n"
	              "    2)\n"
	              "  print(x)\n"));
	ACCEPTED(MAIN("  print(\n"
	              "    1\n"
	              "    )\n"));
	ACCEPTED("import ifj26.*\n"
	         "def f(\n"
	         "  a: Int,\n"
	         "  b: Int\n"
	         "  ): Int = a\n");
	FINISH();
}

static void trailing_comma_needs_line_break(void **state) {
	(void)state;
	// trailing comma is ok when a newline is between it and the closing paren
	ACCEPTED("import ifj26.*\n"
	         "def f(\n"
	         "  a: Int,\n"
	         "  b: Int,\n"
	         "  ): Int = a\n");
	ACCEPTED(MAIN("  print(\n"
	              "    1,\n"
	              "    )\n"));
	// only comments and whitespace can be between them
	ACCEPTED(MAIN("  print(\n"
	              "    1, // one\n"
	              "    )\n"));
	// on one line its an error
	SYNTAX_ERROR("import ifj26.*\n"
	             "def f(a: Int,): Int = a\n");
	SYNTAX_ERROR(MAIN("  print(1,)\n"));
	// and the list cant be empty
	SYNTAX_ERROR(MAIN("  print(\n"
	                  "    ,\n"
	                  "    )\n"));
	FINISH();
}

static void expression_cant_continue_on_next_line(void **state) {
	(void)state;
	// unfinished expression cant go on on the next logical line
	SYNTAX_ERROR(MAIN("  val x = 1 +\n"
	                  "  2\n"));
	SYNTAX_ERROR(MAIN("  val x = 1 +\n"
	                  "    2\n"));
	// logical line cant start with an operator
	SYNTAX_ERROR(MAIN("  val x = 1\n"
	                  "  * 2\n"));
	SYNTAX_ERROR(MAIN("  val x = 1\n"
	                  "  == 2\n"));
	FINISH();
}

static void blocks_are_defined_by_indentation(void **state) {
	(void)state;
	// block has to be indented more than its header
	SYNTAX_ERROR("import ifj26.*\n"
	             "@main def main(): Unit =\n"
	             "print(1)\n");
	// all statements of a block have the same indentation
	SYNTAX_ERROR(MAIN("  print(1)\n"
	                  "    print(2)\n"));
	// deeper blocks work
	ACCEPTED(MAIN("  while true do\n"
	              "      print(1)\n"
	              "      print(2)\n"
	              "  print(3)\n"));
	FINISH();
}

static void blocks_cant_be_empty(void **state) {
	(void)state;
	SYNTAX_ERROR("import ifj26.*\n"
	             "@main def main(): Unit =\n");
	SYNTAX_ERROR("import ifj26.*\n"
	             "def f(): Unit =\n"
	             "@main def main(): Unit =\n"
	             "  print(1)\n");
	SYNTAX_ERROR(MAIN("  while true do\n"
	                  "  print(1)\n"));
	// () can stand for an empty body, example from section 2.1
	ACCEPTED(MAIN("  val x = 1\n"
	              "  if x < 5 then\n"
	              "    print(\"prilis malo\")\n"
	              "  else\n"
	              "    ()\n"));
	FINISH();
}

static void blank_and_comment_lines_are_ignored(void **state) {
	(void)state;
	ACCEPTED(MAIN("  print(1)\n"
	              "\n"
	              "      \n"
	              "// comment at column 0\n"
	              "        // deeply indented comment\n"
	              "  print(2)\n"));
	ACCEPTED(MAIN("  print(1)\n"
	              "\n"
	              "\n"));
	FINISH();
}

static void tab_indentation_is_lexical_error(void **state) {
	(void)state;
	LEXICAL_ERROR("import ifj26.*\n"
	              "@main def main(): Unit =\n"
	              "\tprint(1)\n");
	FINISH();
}

/*
  COMMENTS
*/

static void comments(void **state) {
	(void)state;
	// line comment works as an end of line
	ACCEPTED(MAIN("  print(1) // comment\n"
	              "  print(2)\n"));
	ACCEPTED(MAIN("  print(1) // comment at the end of file"));
	// block comment without newline is just whitespace
	ACCEPTED(MAIN("  val x = /* one */ 1\n"
	              "  print(x)\n"));
	// block comment with a newline works as an end of line
	ACCEPTED(MAIN("  print(1) /* first\n"
	              "     second */\n"
	              "  print(2)\n"));
	// block comments can be nested
	ACCEPTED(MAIN("  /* outer /* inner */ still outer */\n"
	              "  print(1)\n"));
	// unclosed block comment is a lexical error
	LEXICAL_ERROR(MAIN("  print(1) /* never closed\n"));
	LEXICAL_ERROR(MAIN("  /* outer /* inner */ only inner closed\n"
	                   "  print(1)\n"));
	FINISH();
}

/*
  LITERALS
*/

static void literals(void **state) {
	(void)state;
	ACCEPTED(MAIN("  val a = 42\n"
	              "  val b = 1_000_000\n"
	              "  val c = 0\n"
	              "  val d = -5\n"
	              "  print(a)\n"));
	ACCEPTED(MAIN("  val a = 3.14\n"
	              "  val b = .5\n"
	              "  val c = 1e10\n"
	              "  val d = 2.5E-3\n"
	              "  val e = 1.5d\n"
	              "  val f = 1_0.2_5\n"
	              "  print(a)\n"));
	ACCEPTED(MAIN("  val s = \"tab\\t quote\\\" slash\\\\ new\\n ret\\r\"\n"
	              "  print(s)\n"));
	ACCEPTED(MAIN("  val s = \"\"\"A\n"
	              "    hoj s\\nvete\n"
	              "     \"\"\"\n"
	              "  print(s)\n"));
	ACCEPTED(MAIN("  val u = ()\n"
	              "  print(u)\n"));
	FINISH();
}

static void bad_literals_are_lexical_errors(void **state) {
	(void)state;
	// non zero literal cant start with 0
	LEXICAL_ERROR(MAIN("  val a = 012\n"));
	// unknown escape sequence
	LEXICAL_ERROR(MAIN("  val s = \"\\q\"\n"));
	// unterminated string, and a string cant contain a real newline
	LEXICAL_ERROR(MAIN("  val s = \"abc\n"));
	LEXICAL_ERROR(MAIN("  val s = \"\"\"never closed\n"));
	// unknown character
	LEXICAL_ERROR(MAIN("  val a = 1 $ 2\n"));
	FINISH();
}

/*
  STATEMENTS
*/

static void variable_definitions(void **state) {
	(void)state;
	ACCEPTED(MAIN("  val a = 1\n"
	              "  var b: Int = 2\n"
	              "  val c: Double = 1.5\n"
	              "  var d: String = \"x\"\n"
	              "  val e: Unit = ()\n"
	              "  print(a)\n"));
	// initialisation is always required
	SYNTAX_ERROR(MAIN("  val a: Int\n"));
	SYNTAX_ERROR(MAIN("  var a\n"));
	SYNTAX_ERROR(MAIN("  val a =\n"));
	SYNTAX_ERROR(MAIN("  val a: = 1\n"));
	SYNTAX_ERROR(MAIN("  val = 1\n"));
	// keywords cant be used as identifiers
	SYNTAX_ERROR(MAIN("  val if = 1\n"));
	SYNTAX_ERROR(MAIN("  var Int = 1\n"));
	SYNTAX_ERROR(MAIN("  val return = 1\n"));
	FINISH();
}

static void assignments(void **state) {
	(void)state;
	ACCEPTED(MAIN("  var x = 1\n"
	              "  x = x + 1\n"
	              "  x = f(x)\n"));
	// assigning to an undefined or immutable variable is error 7 or 3, still fine for syntax
	ACCEPTED(MAIN("  val y = 1\n"
	              "  y = 2\n"
	              "  z = 3\n"));
	SYNTAX_ERROR(MAIN("  var x = 1\n"
	                  "  x =\n"));
	SYNTAX_ERROR(MAIN("  1 = 2\n"));
	SYNTAX_ERROR(MAIN("  var x = 1\n"
	                  "  x + 1 = 2\n"));
	FINISH();
}

static void if_statements(void **state) {
	(void)state;
	// base form from section 2.4
	ACCEPTED(MAIN("  val x = 1\n"
	              "  if x < 5 then\n"
	              "    print(1)\n"
	              "  else\n"
	              "    print(2)\n"));
	// else has to be on its own logical line indented like the if
	SYNTAX_ERROR(MAIN("  val x = 1\n"
	                  "  if x < 5 then\n"
	                  "    print(1)\n"
	                  "    else\n"
	                  "    print(2)\n"));
	SYNTAX_ERROR(MAIN("  if x < 5\n"
	                  "    print(1)\n"));
	SYNTAX_ERROR(MAIN("  if then\n"
	                  "    print(1)\n"));
	SYNTAX_ERROR(MAIN("  else\n"
	                  "    print(1)\n"));
	FINISH();
}

static void while_loops(void **state) {
	(void)state;
	ACCEPTED(MAIN("  var i = 0\n"
	              "  while i < 10 do\n"
	              "    i = i + 1\n"));
	SYNTAX_ERROR(MAIN("  while i < 10\n"
	                  "    i = i + 1\n"));
	SYNTAX_ERROR(MAIN("  while do\n"
	                  "    i = i + 1\n"));
	FINISH();
}

static void return_statements(void **state) {
	(void)state;
	ACCEPTED("import ifj26.*\n"
	         "def f(a: Int): Int =\n"
	         "  return a * 2\n"
	         "def g(): Unit =\n"
	         "  return\n"
	         "def h(): Int =\n"
	         "  return h()\n"
	         "@main def main(): Unit =\n"
	         "  print(f(1))\n");
	FINISH();
}

static void calls_as_statements(void **state) {
	(void)state;
	ACCEPTED(MAIN("  print(\"hi\")\n"
	              "  print(readLine())\n"
	              "  f()\n"));
	SYNTAX_ERROR(MAIN("  print(\"hi\"\n"));
	SYNTAX_ERROR(MAIN("  print \"hi\")\n"));
	FINISH();
}

/*
  EXPRESSIONS
*/

static void operators_and_precedence(void **state) {
	(void)state;
	ACCEPTED(MAIN("  val a = 1 + 2 * 3 - 4 / 5\n"
	              "  val b = (1 + 2) * (3 - 4)\n"
	              "  val c = -a * -b\n"
	              "  val d = \"a\" + \"b\"\n"
	              "  val e = a < b == c >= d\n"
	              "  val f = a != b\n"
	              "  print(a)\n"));
	FINISH();
}

static void relational_operators_dont_chain(void **state) {
	(void)state;
	// relational and equality operators have no associativity
	SYNTAX_ERROR(MAIN("  val a = 1 < 2 < 3\n"));
	SYNTAX_ERROR(MAIN("  val a = 1 <= 2 > 3\n"));
	SYNTAX_ERROR(MAIN("  val a = 1 == 2 != 3\n"));
	SYNTAX_ERROR(MAIN("  if 1 < 2 < 3 then\n"
	                  "    print(1)\n"));
	FINISH();
}

static void malformed_expressions(void **state) {
	(void)state;
	SYNTAX_ERROR(MAIN("  val a = 1 +\n"));
	SYNTAX_ERROR(MAIN("  val a = * 2\n"));
	SYNTAX_ERROR(MAIN("  val a = (1 + 2\n"));
	SYNTAX_ERROR(MAIN("  val a = 1 + 2)\n"));
	SYNTAX_ERROR(MAIN("  val a = 1 2\n"));
	SYNTAX_ERROR(MAIN("  val a = 1 * / 2\n"));
	SYNTAX_ERROR(MAIN("  val a = ( )+\n"));
	FINISH();
}

/*
  FUNEXP
*/

static void funexp_calls_in_expressions(void **state) {
	(void)state;
	// the example from section 5.1
	ACCEPTED("import ifj26.*\n"
	         "def factorial(n: Int): Int =\n"
	         "  if n < 2 then\n"
	         "    return 1\n"
	         "  else\n"
	         "    return n * factorial(n - 1)\n"
	         "@main def main(): Unit =\n"
	         "  val result = 2 * factorial(readInt() + 1)\n"
	         "  print(toStr(result + strLength(subStr(\"abcd\", 1, 3))))\n");
	SYNTAX_ERROR(MAIN("  val a = f(1 2)\n"));
	SYNTAX_ERROR(MAIN("  val a = f(,)\n"));
	SYNTAX_ERROR(MAIN("  val a = f(1 +)\n"));
	FINISH();
}

/*
  BOOLTHEN
*/

static void boolthen_values(void **state) {
	(void)state;
	ACCEPTED(MAIN("  val t: Boolean = true\n"
	              "  val f = false\n"
	              "  val a = !t && f || t == f\n"
	              "  val b = 1 < 2 && 3 >= 4\n"
	              "  print(a)\n"));
	ACCEPTED(MAIN("  var go = true\n"
	              "  while go do\n"
	              "    go = false\n"));
	SYNTAX_ERROR(MAIN("  val a = true && || false\n"));
	SYNTAX_ERROR(MAIN("  val a = !\n"));
	FINISH();
}

static void boolthen_if_without_else_and_else_if(void **state) {
	(void)state;
	// else is optional
	ACCEPTED(MAIN("  val x = 1\n"
	              "  if x < 0 then\n"
	              "    print(\"negative\")\n"
	              "  print(x)\n"));
	// the else if example from section 5.2
	ACCEPTED(MAIN("  val x = 1\n"
	              "  if x < 0 then\n"
	              "    print(\"negative\")\n"
	              "  else if x == 0 then\n"
	              "    print(\"zero\")\n"));
	ACCEPTED(MAIN("  val x = 1\n"
	              "  if x < 0 then\n"
	              "    print(\"negative\")\n"
	              "  else if x == 0 then\n"
	              "    print(\"zero\")\n"
	              "  else\n"
	              "    print(\"positive\")\n"));
	FINISH();
}

/*===============
  CONTROL, SECTION 5.3
  ===============*/

static void control_if_expressions(void **state) {
	(void)state;
	// the examples from section 5.3
	ACCEPTED(MAIN("  var x = -5\n"
	              "  val absolute = if x < 0 then 0 - x else x\n"
	              "  val abs2 = if x < 0 then\n"
	              "    0 - x\n"
	              "  else\n"
	              "    x\n"
	              "  val abs3 = if x < 0 then 0 - x else\n"
	              "      x\n"
	              "  print(absolute)\n"));
	// if expression inside a larger expression
	ACCEPTED(MAIN("  val x = 1\n"
	              "  val y = 1 + (if x < 0 then 1 else 2)\n"
	              "  print(y)\n"));
	ACCEPTED("import ifj26.*\n"
	         "def sign(x: Int): Int = if x < 0 then -1 else if x == 0 then 0 else 1\n");
	FINISH();
}

static void control_if_expression_needs_else(void **state) {
	(void)state;
	// when if is used as an expression the else branch is required
	SYNTAX_ERROR(MAIN("  val x = 1\n"
	                  "  val y = 1 + (if x < 0 then 1)\n"));
	SYNTAX_ERROR(MAIN("  val x = 1\n"
	                  "  val y = if x < 0 then 1\n"));
	SYNTAX_ERROR("import ifj26.*\n"
	             "def f(x: Int): Int = if x < 0 then 1\n");
	FINISH();
}

static void control_one_line_branches(void **state) {
	(void)state;
	ACCEPTED(MAIN("  var x = 1\n"
	              "  if x < 0 then x = 0 else x = 1\n"
	              "  if x < 0 then print(1)\n"
	              "  if x < 0 then\n"
	              "    print(1)\n"
	              "  else print(2)\n"
	              "  if x < 0 then print(1)\n"
	              "  else\n"
	              "    print(2)\n"));
	ACCEPTED(MAIN("  var i = 0\n"
	              "  while i < 3 do i = i + 1\n"));
	FINISH();
}

static void control_for_loops(void **state) {
	(void)state;
	// the examples from section 5.3
	ACCEPTED(MAIN("  for i <- 1 to 10 by 2 do\n"
	              "    print(i)\n"
	              "  val limit = 10\n"
	              "  for\n"
	              "    i <- 0 until limit\n"
	              "    if i != 3\n"
	              "    if i < 8\n"
	              "  do\n"
	              "    print(i)\n"));
	// one line bodies and guards
	ACCEPTED(MAIN("  for i <- 0 until 5 do print(i)\n"
	              "  for i <- 1 to 9 if i != 3 if i < 8 do print(i)\n"
	              "  for i <- 10 to 1 by -1 do\n"
	              "    print(i)\n"));
	ACCEPTED(MAIN("  for\n"
	              "    i <- 1 to 3\n"
	              "  do print(i)\n"));
	FINISH();
}

static void control_for_loop_errors(void **state) {
	(void)state;
	SYNTAX_ERROR(MAIN("  for i = 1 to 3 do\n"
	                  "    print(i)\n"));
	SYNTAX_ERROR(MAIN("  for i <- 1 to do\n"
	                  "    print(i)\n"));
	SYNTAX_ERROR(MAIN("  for i <- 1 to 3\n"
	                  "    print(i)\n"));
	SYNTAX_ERROR(MAIN("  for i <- 1 to 3 by do\n"
	                  "    print(i)\n"));
	// more generators and yield are not supported
	SYNTAX_ERROR(MAIN("  for\n"
	                  "    i <- 1 to 3\n"
	                  "    j <- 1 to 3\n"
	                  "  do\n"
	                  "    print(i)\n"));
	SYNTAX_ERROR(MAIN("  for i <- 1 to 3 yield i\n"));
	// do in the multi line form has the indentation of for
	SYNTAX_ERROR(MAIN("  for\n"
	                  "    i <- 1 to 3\n"
	                  "    do\n"
	                  "    print(i)\n"));
	// generator has to be indented more than for
	SYNTAX_ERROR(MAIN("  for\n"
	                  "  i <- 1 to 3\n"
	                  "  do\n"
	                  "    print(i)\n"));
	FINISH();
}

/*
  VALMEM
*/

static void valmem_members(void **state) {
	(void)state;
	ACCEPTED(MAIN("  val number = 2.5\n"
	              "  val a = \"abc\".length\n"
	              "  val b = number.floor.toInt\n"
	              "  val c = (1.2345).toString\n"
	              "  val d = number.toInt + a\n"
	              "  print(b.toString)\n"));
	SYNTAX_ERROR(MAIN("  val a = \"abc\".\n"));
	SYNTAX_ERROR(MAIN("  val a = \"abc\".1\n"));
	FINISH();
}

/*
  MAIN
*/

int main(void) {
	int test_results = 0;

	const struct CMUnitTest structure_tests[] = {
	    SPEC_TEST(spec_factorial_example),  SPEC_TEST(prolog_is_required_once),
	    SPEC_TEST(prolog_has_exact_form),   SPEC_TEST(main_annotation_placement),
	    SPEC_TEST(main_rules_are_semantic), SPEC_TEST(only_functions_at_top_level),
	    SPEC_TEST(function_forms),          SPEC_TEST(function_header_errors),
	};

	const struct CMUnitTest layout_tests[] = {
	    SPEC_TEST(semicolon_separates_statements),
	    SPEC_TEST(no_separator_needed_at_the_end),
	    SPEC_TEST(newlines_inside_parentheses_are_whitespace),
	    SPEC_TEST(trailing_comma_needs_line_break),
	    SPEC_TEST(expression_cant_continue_on_next_line),
	    SPEC_TEST(blocks_are_defined_by_indentation),
	    SPEC_TEST(blocks_cant_be_empty),
	    SPEC_TEST(blank_and_comment_lines_are_ignored),
	    SPEC_TEST(tab_indentation_is_lexical_error),
	    SPEC_TEST(comments),
	    SPEC_TEST(literals),
	    SPEC_TEST(bad_literals_are_lexical_errors),
	};

	const struct CMUnitTest statement_tests[] = {
	    SPEC_TEST(variable_definitions), SPEC_TEST(assignments),       SPEC_TEST(if_statements),
	    SPEC_TEST(while_loops),          SPEC_TEST(return_statements), SPEC_TEST(calls_as_statements),
	};

	const struct CMUnitTest expression_tests[] = {
	    SPEC_TEST(operators_and_precedence),
	    SPEC_TEST(relational_operators_dont_chain),
	    SPEC_TEST(malformed_expressions),
	};

	const struct CMUnitTest extension_tests[] = {
	    SPEC_TEST(funexp_calls_in_expressions),
	    SPEC_TEST(boolthen_values),
	    SPEC_TEST(boolthen_if_without_else_and_else_if),
	    SPEC_TEST(control_if_expressions),
	    SPEC_TEST(control_if_expression_needs_else),
	    SPEC_TEST(control_one_line_branches),
	    SPEC_TEST(control_for_loops),
	    SPEC_TEST(control_for_loop_errors),
	    SPEC_TEST(valmem_members),
	};

	EXEC_TEST_CATEGORY("PROGRAM STRUCTURE (2, 2.3)", structure_tests);
	EXEC_TEST_CATEGORY("LEXICAL UNITS AND LAYOUT (1.1, 1.2)", layout_tests);
	EXEC_TEST_CATEGORY("STATEMENTS (2.2, 2.4, 2.5)", statement_tests);
	EXEC_TEST_CATEGORY("EXPRESSIONS (3)", expression_tests);
	EXEC_TEST_CATEGORY("EXTENSIONS (5.1 to 5.4)", extension_tests);

	return test_results;
}
