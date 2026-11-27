/*
AI PROMPT:
Look at file "src/tests/test_syntax_semantic_integration.c" and the lexer, parser,
semantic actions and analyser it uses. The tests compile full programs and check the specified result.
They should cover examples from IFJ26_examples.zip in this folder, program and function rules,
variables and scopes, built-ins, expressions, and the FUNEXP, BOOLTHEN, CONTROL and VALMEM extensions. 
They also check semantic errors like undefined or duplicate names, bad arguments and types, writes to
immutable variables and invalid loop steps, along with AST handoff and error precedence.
Please review the suite and update the checks where they don't reflect that behavior.
Keep the expected results aligned with the compiler pipeline and semantic rules listed here. 
Don't hallucinate functionality that has not been defined by this prompt, keep rechecking this prompt
repeatedly after any action to make sure you are only defining tests for the existing implementation.
 */
/**
 * @file test_syntax_semantic_integration.c
 * @author xkohela00
 * @brief Integration tests of the syntax and semantic analyser.
 *
 * Every program goes through the real lexer, the parser, the semantic actions that build the AST and
 * the semantic analyser, the same way the compiler runs them. Expected exit codes come from the IFJ26
 * specification (section numbers are in the test names), never from what the code does today.
 * Implemented extensions: FUNEXP, BOOLTHEN, CONTROL, VALMEM.
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

#include "../include/ast.h"
#include "../include/functable.h"
#include "../include/lexer.h"
#include "../include/semantic_anal.h"
#include "../include/syntax_anal.h"

// HELPERS

#define PROLOG "import ifj26.*\n"
// program with only @main, body has to be indented by two spaces
#define MAIN(body) PROLOG "@main def main(): Unit =\n" body
// program with other function definitions before @main
#define WITH(definitions, body) PROLOG definitions "@main def main(): Unit =\n" body

/**
 * @brief Runs the lexer, the parser and the semantic analyser on a source text.
 *
 * @param source Program text.
 * @return Exit code the compiler would give: 0, 1, 2, 3 to 10 or 99.
 */
static int compile(const char *source) {
	FILE *file = tmpfile();
	if (file == NULL || fputs(source, file) == EOF || fflush(file) != 0 || fseek(file, 0, SEEK_SET) != 0) {
		if (file != NULL)
			fclose(file);
		return 99;
	}

	lexer_t lexer;
	int result = 0;
	const lexer_result_t lexer_result = lexer_init(&lexer, file);
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

static int mismatches; // wrong results in the current test, every case runs before the test fails

/**
 * @brief Compiles one program and remembers a wrong exit code instead of stopping the test.
 *
 * @param expected Exit code required by the specification.
 * @param source Program text.
 * @param line Line of the EXPECT in this file, printed on a mismatch.
 */
static void check_source(int expected, const char *source, int line) {
	const int got = compile(source);
	sem_actions_free();
	if (got != expected) {
		mismatches++;
		print_error("line %d: expected %d, got %d for:\n----\n%s----\n", line, expected, got, source);
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

// OFFICIAL EXAMPLES

// basic/factorial.scala from IFJ26_examples.zip
static const char *const EXAMPLE_FACTORIAL =
    "// Zakladni zadani: vstup, val/var, podminky, cyklus, funkce, rekurze.\n"
    "import ifj26.*\n"
    "\n"
    "def factorialIterative(n: Int): Int =\n"
    "  var result = 1  // promenne\n"
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
    "    val smaller = factorialRecursive(previous)  // v zakl. zadani nelze vyraz pouzit jako arg. funkce\n"
    "    return n * smaller\n"
    "\n"
    "@main def factorialExample(): Unit =\n"
    "  val n = readInt()  // nemodifikovatelna promenna\n"
    "  // Int je ve Scale 32b - vice nez 12! by preteklo.\n"
    "  // V IFJ chovani v krajnich situacich (pre-/podteceni) prilis netestujeme,\n"
    "  // ale je vhodne se k tomu nejak konkretne pri implementaci postavit.\n"
    "  if n < 0 then\n"
    "    print(\"Use an integer from 0 to 12.\\n\")\n"
    "  else\n"
    "    if n > 12 then\n"
    "      print(\"Use an integer from 0 to 12.\\n\")\n"
    "    else\n"
    "      val iterative = factorialIterative(n)\n"
    "      val recursive = factorialRecursive(n)\n"
    "      print(\"iterative: \")\n"
    "      print(iterative)\n"
    "      print(\"\\nrecursive: \")\n"
    "      print(recursive)\n"
    "      print(\"\\n\")\n";

// basic/strings.scala from IFJ26_examples.zip
static const char *const EXAMPLE_STRINGS =
    "// Zakladni zadani: retezce, vestavene funkce, return, implicitni konverze Int->Double\n"
    "import ifj26.*\n"
    "\n"
    "def withoutLineFeed(text: String): String =\n"
    "  val length = strLength(text)\n"
    "  if length == 0 then\n"
    "    return text\n"
    "  else\n"
    "    val last = length - 1\n"
    "    val code = ord(text, last)\n"
    "    if code == 10 then\n"
    "      return subStr(text, 0, last)\n"
    "    else\n"
    "      return text\n"
    "\n"
    "def reverse(text: String): String =\n"
    "  val length = strLength(text)\n"
    "  var index = length - 1\n"
    "  var result = \"\"\n"
    "  while index >= 0 do\n"
    "    val code = ord(text, index)\n"
    "    val character = chr(code)\n"
    "    result = result + character // konkatenace\n"
    "    index = index - 1\n"
    "  result\n"
    "\n"
    "def prefix(text: String): String =\n"
    "  val length = strLength(text)\n"
    "  var end = length\n"
    "  if length > 3 then\n"
    "    end = 3\n"
    "  else  // podminka ma v zakladnim zadani vzdy obe vetve\n"
    "    ()\n"
    "  if length == 0 then\n"
    "    return \"\"\n"
    "  else\n"
    "    return subStr(text, 0, end)\n"
    "\n"
    "// length se implicitne prevede na Double, protoze prava strana deleni\n"
    "// obsahuje Double\n"
    "def halfLength(length: Int): Double = length / 2.0\n"
    "\n"
    "def showLine(text: String): Unit =\n"
    "  print(text)\n"
    "  print(\"\\n\")\n"
    "\n"
    "@main def stringsExample(): Unit =\n"
    "  val raw = readLine()\n"
    "  val text = withoutLineFeed(raw)\n"
    "  val backwards = reverse(text)\n"
    "  val first = prefix(text)\n"
    "  val length = strLength(text)\n"
    "  val half = halfLength(length)\n"
    "  val halfText = toStr(half)\n"
    "  showLine(backwards)\n"
    "  showLine(first)\n"
    "  print(\"length: \")\n"
    "  print(length)\n"
    "  print(\"\\nhalf: \")\n"
    "  showLine(halfText)\n";

// extensions/boolthen.scala from IFJ26_examples.zip
static const char *const EXAMPLE_BOOLTHEN = "// Rozsireni BOOLTHEN\n"
                                            "import ifj26.*\n"
                                            "\n"
                                            "def negate(value: Boolean): Boolean = !value\n"
                                            "\n"
                                            "@main def boolthenExample(): Unit =\n"
                                            "  val enabled = readBoolean()\n"
                                            "  var divisor = 0\n"
                                            "  if enabled then\n"
                                            "    divisor = 2\n"
                                            "  else\n"
                                            "    ()\n"
                                            "  val safe = divisor != 0 && (10 / divisor > 1)\n"
                                            "  val inverted = negate(enabled)\n"
                                            "  val either = enabled || inverted\n"
                                            "  print(\"safe: \")\n"
                                            "  print(safe)\n"
                                            "  print(\"\\neither: \")\n"
                                            "  print(either)\n"
                                            "  print(\"\\n\")\n"
                                            "  if safe then\n"
                                            "    print(\"ready\\n\")\n"
                                            "  else if enabled then\n"
                                            "    print(\"blocked\\n\")\n"
                                            "  else\n"
                                            "    print(\"disabled\\n\")\n"
                                            "  if !enabled then  // podpora podminek bez vetve else\n"
                                            "    print(\"input was false\\n\")\n";

// extensions/control.scala from IFJ26_examples.zip
static const char *const EXAMPLE_CONTROL =
    "// Rozsireni CONTROL (+ vyzadovane BOOLTHEN).\n"
    "import ifj26.*\n"
    "\n"
    "@main def controlExample(): Unit =\n"
    "  val limit = 9\n"
    "  val step = 2\n"
    "  val sign = if limit < 0 then -1 else if limit == 0 then 0 else 1  // podminka jako vyraz\n"
    "  var sum = 0\n"
    "  for  // for cyklus se strazemi\n"
    "    i <- 1 to limit by step\n"
    "    if i != 5\n"
    "    if i < 9\n"
    "  do\n"
    "    sum = sum + i\n"
    "  print(\"sign: \")\n"
    "  print(sign)\n"
    "  print(\"\\nsum: \")\n"
    "  print(sum)\n"
    "  print(\"\\ndescending:\")\n"
    "  for i <- 6 to 2 by -2 do\n"
    "    print(\" \")\n"
    "    print(i)\n"
    "  print(\"\\n\")\n"
    "  print(\"exclusive:\")\n"
    "  for i <- 0 until 3 do // jednoduchy for cyklus\n"
    "    print(\" \")\n"
    "    print(i)\n"
    "  print(\"\\n\")\n";

// extensions/funexp.scala from IFJ26_examples.zip
static const char *const EXAMPLE_FUNEXP =
    "// Rozsireni FUNEXP\n"
    "import ifj26.*\n"
    "\n"
    "def traced(value: Int): Int =\n"
    "  print(\"[\")\n"
    "  print(value)\n"
    "  print(\"]\")\n"
    "  value\n"
    "\n"
    "@main def funexpExample(): Unit =\n"
    "  val result = traced(2) + 3 * traced(traced(4) + 1)  // vyrazy jako argumenty, volani uvnitr vyrazu\n"
    "  // na vystupu si povsimnete, v jakem poradi jsou funkce volany!\n"
    "  print(\"\\nresult: \")\n"
    "  print(toStr(result))\n"
    "  print(\"\\n\")\n";

// extensions/valmem.scala from IFJ26_examples.zip
static const char *const EXAMPLE_VALMEM = "// Rozsireni VALMEM\n"
                                          "import ifj26.*\n"
                                          "\n"
                                          "@main def valmemExample(): Unit =\n"
                                          "  val number = -3.75\n"
                                          "  val truncated = number.toInt\n"
                                          "  val floored = number.floor\n"
                                          "  val floorInt = number.floor.toInt\n"
                                          "  val digits = truncated.toString\n"
                                          "  val length = \"IFJ26\".length\n"
                                          "  val done: Unit = ()\n"
                                          "  val unitText = done.toString\n"
                                          "  print(\"truncated: \")\n"
                                          "  print(digits)\n"
                                          "  print(\"\\nfloor: \")\n"
                                          "  print(floored)\n"
                                          "  print(\"\\nfloor Int: \")\n"
                                          "  print(floorInt)\n"
                                          "  print(\"\\nlength: \")\n"
                                          "  print(length)\n"
                                          "  print(\"\\nUnit: \")\n"
                                          "  print(unitText)\n"
                                          "  print(\"\\n\")\n";

static void official_examples_are_valid(void **state) {
	(void)state;
	EXPECT(0, EXAMPLE_FACTORIAL);
	EXPECT(0, EXAMPLE_STRINGS);
	EXPECT(0, EXAMPLE_BOOLTHEN);
	EXPECT(0, EXAMPLE_CONTROL);
	EXPECT(0, EXAMPLE_FUNEXP);
	EXPECT(0, EXAMPLE_VALMEM);
	FINISH();
}

// the code generator gets the checked AST and the function table, every example has to provide them
static void official_examples_hand_over_ast(void **state) {
	(void)state;
	const char *const examples[] = {EXAMPLE_FACTORIAL, EXAMPLE_STRINGS, EXAMPLE_BOOLTHEN,
	                                EXAMPLE_CONTROL,   EXAMPLE_FUNEXP,  EXAMPLE_VALMEM};
	for (size_t i = 0; i < sizeof(examples) / sizeof(examples[0]); i++) {
		assert_int_equal(compile(examples[i]), 0);
		assert_int_equal(sem_actions_result(), SEM_OK);

		ast_t *ast = NULL;
		funct_t *functions = NULL;
		assert_true(sem_actions_take(&ast, &functions));
		assert_non_null(functions);
		ast_node_t *root = ast_root(ast);
		assert_non_null(root);
		assert_int_equal(root->kind, AST_NODE_PROGRAM);
		assert_int_equal(ast_node_child_at(root, 0)->kind, AST_NODE_IMPORT);

		// exactly one @main function
		size_t main_count = 0;
		for (ast_node_t *child = root->first_child; child != NULL; child = child->next_sibling)
			main_count += child->kind == AST_NODE_MAIN_FUNCTION;
		assert_int_equal(main_count, 1);

		ast_destroy(&ast);
		funct_free(functions);
		sem_actions_free();
	}
}

// PROGRAM STRUCTURE

static void structure_entry_function_2(void **state) {
	(void)state;
	// the spec example from section 2
	EXPECT(0, PROLOG "def factorial(n: Int): Int =\n"
	                 "  var result = 1\n"
	                 "  var i: Int = n // uvedeni typu je nepovinne\n"
	                 "  while i > 1 do\n"
	                 "    result = result * i\n"
	                 "    i = i - 1\n"
	                 "  result\n"
	                 "\n"
	                 "@main def runFoo(): Unit =\n"
	                 "  val n = readInt()\n"
	                 "  val result = factorial(n)\n"
	                 "  print(result)\n"
	                 "  print(\"\\n\")\n");
	// annotation on its own line, any name of the entry function
	EXPECT(0, PROLOG "@main\ndef start(): Unit =\n  print(1)\n");
	EXPECT(0, PROLOG "@main def start(): Unit = print(1)\n");
	// missing @main is 3, two of them 4, a parameter 5, another return type 6
	EXPECT(3, PROLOG "def f(): Unit = ()\n");
	EXPECT(4, PROLOG "@main def a(): Unit = ()\n@main def b(): Unit = ()\n");
	EXPECT(5, PROLOG "@main def main(a: Int): Unit = ()\n");
	EXPECT(6, PROLOG "@main def main(): Int = 1\n");
	EXPECT(6, PROLOG "@main def main(): String = \"a\"\n");
	FINISH();
}

static void structure_prolog_2(void **state) {
	(void)state;
	EXPECT(2, "@main def main(): Unit = ()\n");
	EXPECT(2, PROLOG PROLOG "@main def main(): Unit = ()\n");
	EXPECT(0, "// comment\n/* block */\n" PROLOG "@main def main(): Unit = ()\n");
	FINISH();
}

// VARIABLES AND SCOPES

static void variables_definition_and_types_2_2(void **state) {
	(void)state;
	EXPECT(0, MAIN("  val a = 1\n  var b: Int = 2\n  b = a + b\n  print(b)\n"));
	EXPECT(0, MAIN("  val s: String = \"text\"\n  val d: Double = 1.5\n  print(s)\n  print(d)\n"));
	EXPECT(0, MAIN("  val u: Unit = ()\n  val v: Unit = print(1)\n  val w = print(2)\n"));
	EXPECT(0, MAIN("  val b: Boolean = true\n  var c = false\n  c = b\n"));
	// the only implicit conversion is Int to Double
	EXPECT(0, MAIN("  val d: Double = 1\n"));
	EXPECT(6, MAIN("  val i: Int = 1.5\n"));
	EXPECT(6, MAIN("  val s: String = 1\n"));
	EXPECT(6, MAIN("  val i: Int = \"1\"\n"));
	EXPECT(6, MAIN("  val u: Unit = 1\n"));
	EXPECT(6, MAIN("  val b: Boolean = 1\n"));
	FINISH();
}

static void variables_assignment_2_2(void **state) {
	(void)state;
	// the type of a variable never changes
	EXPECT(0, MAIN("  var d = 1.5\n  d = 2\n"));
	EXPECT(6, MAIN("  var i = 1\n  i = 2.5\n"));
	EXPECT(6, MAIN("  var s = \"a\"\n  s = 1\n"));
	EXPECT(6, MAIN("  var b = true\n  b = 1\n"));
	// only a mutable variable can be assigned
	EXPECT(7, MAIN("  val x = 1\n  x = 2\n"));
	EXPECT(7, WITH("def f(a: Int): Unit =\n  a = 2\n", "  f(1)\n"));
	EXPECT(7, MAIN("  val x = 1\n  if x > 0 then\n    x = 2\n  else\n    ()\n"));
	FINISH();
}

static void variables_scopes_2_2(void **state) {
	(void)state;
	// visible in sub-blocks, a sub-block may shadow it
	EXPECT(0, MAIN("  val x = 1\n  while x < 0 do\n    print(x)\n"));
	EXPECT(0, MAIN("  val x = 1\n"
	               "  if x > 0 then\n"
	               "    val x = \"inner\"\n"
	               "    print(x)\n"
	               "  else\n"
	               "    ()\n"
	               "  print(x + 1)\n"));
	// the same name in two sibling blocks
	EXPECT(0, MAIN("  if true then\n    val y = 1\n  else\n    val y = \"a\"\n"));
	// redefinition in the same block
	EXPECT(4, MAIN("  val x = 1\n  val x = 2\n"));
	EXPECT(4, MAIN("  val x = 1\n  var x = 2\n"));
	EXPECT(4, MAIN("  if true then\n    var y = 1\n    val y = 2\n  else\n    ()\n"));
	// use before the definition or outside the scope
	EXPECT(3, MAIN("  print(x)\n  val x = 1\n"));
	EXPECT(3, MAIN("  val x = x + 1\n"));
	EXPECT(3, MAIN("  if true then\n    val y = 1\n  else\n    ()\n  print(y)\n"));
	EXPECT(3, MAIN("  var i = 0\n  while i < 3 do\n    val inside = i\n    i = i + 1\n  print(inside)\n"));
	EXPECT(3, WITH("def f(): Unit =\n  val secret = 1\n", "  print(secret)\n"));
	EXPECT(3, WITH("def f(a: Int): Int = a\n", "  print(a)\n"));
	FINISH();
}

// FUNCTIONS

static void functions_definitions_2_3(void **state) {
	(void)state;
	EXPECT(0, WITH("def one(): Int = 1\n", "  print(one())\n"));
	EXPECT(0, WITH("def add(a: Int, b: Int): Int =\n  val c = a + b\n  c\n", "  print(add(1, 2))\n"));
	// called before its definition, mutual recursion with explicit types
	EXPECT(0, PROLOG "@main def main(): Unit =\n  later(1)\ndef later(x: Int): Unit = print(x)\n");
	EXPECT(0, WITH("def isEven(n: Int): Boolean =\n  if n == 0 then true else isOdd(n - 1)\n"
	               "def isOdd(n: Int): Boolean =\n  if n == 0 then false else isEven(n - 1)\n",
	               "  print(isEven(4))\n"));
	// overloading is not allowed, built-in functions count too
	EXPECT(4, WITH("def f(): Unit = ()\ndef f(): Unit = ()\n", "  f()\n"));
	EXPECT(4, WITH("def f(a: Int): Unit = ()\ndef f(a: String): Unit = ()\n", "  f(1)\n"));
	EXPECT(4, WITH("def print(x: Int): Unit = ()\n", "  print(1)\n"));
	EXPECT(4, WITH("def f(a: Int, a: Int): Unit = ()\n", "  f(1, 2)\n"));
	FINISH();
}

static void functions_return_types_2_3(void **state) {
	(void)state;
	EXPECT(0, WITH("def f(): Double = 1\n", "  print(f())\n"));
	EXPECT(6, WITH("def f(): Int = \"a\"\n", "  print(f())\n"));
	EXPECT(6, WITH("def f(): Int = 1.5\n", "  print(f())\n"));
	EXPECT(6, WITH("def f(): String =\n  val s = \"a\"\n  1\n", "  print(f())\n"));
	// inferred return type
	EXPECT(0, WITH("def g() = 1\n", "  val n: Int = g()\n"));
	EXPECT(6, WITH("def g() = 1\n", "  val s: String = g()\n"));
	EXPECT(0, WITH("def g() =\n  var x = 1\n  x = 2\n", "  val u: Unit = g()\n"));
	// recursion and return need an explicit return type
	EXPECT(6, WITH("def f(n: Int) = f(n)\n", "  f(1)\n"));
	EXPECT(6, WITH("def a(n: Int) = b(n)\ndef b(n: Int) = a(n)\n", "  a(1)\n"));
	EXPECT(6, WITH("def f(x: Int) =\n  return x\n", "  f(1)\n"));
	EXPECT(0, WITH("def f(n: Int): Int =\n"
	               "  if n <= 1 then\n"
	               "    return 1\n"
	               "  else\n"
	               "    return n * f(n - 1)\n",
	               "  print(f(5))\n"));
	FINISH();
}

static void functions_body_result_2_3(void **state) {
	(void)state;
	// a block ending with a definition, an assignment or a loop gives ()
	EXPECT(6, WITH("def f(): Int =\n  val x = 1\n", "  f()\n"));
	EXPECT(6, WITH("def f(): Int =\n  var x = 1\n  x = 2\n", "  f()\n"));
	EXPECT(6, WITH("def f(): Int =\n  var i = 0\n  while i < 3 do\n    i = i + 1\n", "  f()\n"));
	// if at the end: every branch returns, or the value of the if expression (CONTROL)
	EXPECT(0,
	       WITH("def f(x: Int): Int =\n  if x > 0 then\n    return 1\n  else\n    return 2\n", "  f(1)\n"));
	EXPECT(0, WITH("def f(x: Int): Int =\n  if x > 0 then\n    1\n  else\n    2\n", "  f(1)\n"));
	// a missing else counts as a branch with ()
	EXPECT(6, WITH("def f(x: Int): Int =\n  if x > 0 then\n    return 1\n", "  f(1)\n"));
	// with an explicit Unit the value is thrown away, return included
	EXPECT(0, WITH("def f(): Unit = 5\n", "  f()\n"));
	EXPECT(0, WITH("def f(): Unit =\n  return 5\n", "  f()\n"));
	EXPECT(0, WITH("def f(): Unit =\n  return\n", "  f()\n"));
	EXPECT(0, WITH("def f(x: Int): Unit =\n  if x > 0 then\n    print(x)\n  else\n    ()\n", "  f(1)\n"));
	// unreachable code is checked too
	EXPECT(6, WITH("def f(): Int =\n  return 1\n  print(1 + \"a\")\n  2\n", "  f()\n"));
	FINISH();
}

static void functions_calls_2_3(void **state) {
	(void)state;
	EXPECT(0, WITH("def f(a: Int, s: String, b: Boolean): Unit = ()\n", "  f(1, \"a\", true)\n"));
	EXPECT(0, WITH("def half(d: Double): Double = d / 2\n", "  print(half(3))\n"));
	EXPECT(5, WITH("def f(a: Int): Int = a\n", "  f()\n"));
	EXPECT(5, WITH("def f(a: Int): Int = a\n", "  f(1, 2)\n"));
	EXPECT(5, WITH("def f(a: Int): Int = a\n", "  f(\"x\")\n"));
	EXPECT(5, WITH("def f(a: Int): Int = a\n", "  f(1.5)\n"));
	EXPECT(5, WITH("def f(a: Boolean): Unit = ()\n", "  f(1)\n"));
	EXPECT(3, MAIN("  foo(1)\n"));
	EXPECT(3, MAIN("  val x = foo()\n"));
	// a call is a statement, an initializer, the right side of an assignment, a body and a return value
	EXPECT(0, WITH("def f(): Int = 1\n"
	               "def g(): Int =\n  return f()\n",
	               "  f()\n  val a = f()\n  var b = 0\n  b = g()\n"));
	FINISH();
}

// BUILT-IN FUNCTIONS

static void builtins_valid_calls_4(void **state) {
	(void)state;
	EXPECT(0, MAIN("  val line: String = readLine()\n"
	               "  val i: Int = readInt()\n"
	               "  val d: Double = readDouble()\n"
	               "  val b: Boolean = readBoolean()\n"
	               "  print(line)\n  print(i)\n  print(d)\n  print(b)\n  print(())\n"));
	EXPECT(0, MAIN("  val a: String = toStr(1)\n"
	               "  val b: String = toStr(1.5)\n"
	               "  val c: String = toStr(\"a\")\n"
	               "  val d: String = toStr(true)\n"
	               "  val e: String = toStr(())\n"));
	EXPECT(0, MAIN("  val n: Int = toInt(1.5)\n"
	               "  val l: Int = strLength(\"abc\")\n"
	               "  val s: String = subStr(\"abcd\", 1, 3)\n"
	               "  val o: Int = ord(\"a\", 0)\n"
	               "  val c: String = chr(65)\n"));
	FINISH();
}

static void builtins_wrong_arguments_4(void **state) {
	(void)state;
	EXPECT(5, MAIN("  print()\n"));
	EXPECT(5, MAIN("  print(1, 2)\n"));
	EXPECT(5, MAIN("  val n = readInt(1)\n"));
	EXPECT(5, MAIN("  val n = strLength(1)\n"));
	EXPECT(5, MAIN("  val s = subStr(\"abc\", 0)\n"));
	EXPECT(5, MAIN("  val s = subStr(\"abc\", 0, \"1\")\n"));
	EXPECT(5, MAIN("  val n = ord(\"a\", \"b\")\n"));
	EXPECT(5, MAIN("  val s = chr(\"a\")\n"));
	EXPECT(5, MAIN("  val n = toInt(\"1\")\n"));
	FINISH();
}

static void builtins_result_types_4(void **state) {
	(void)state;
	EXPECT(6, MAIN("  val s: String = readInt()\n"));
	EXPECT(6, MAIN("  val n: Int = readLine()\n"));
	EXPECT(6, MAIN("  val n: Int = readDouble()\n"));
	EXPECT(6, MAIN("  val n: Int = toStr(1)\n"));
	EXPECT(6, MAIN("  val s: String = print(1)\n"));
	FINISH();
}

// EXPRESSIONS

static void expressions_arithmetic_3(void **state) {
	(void)state;
	EXPECT(0, MAIN("  val i: Int = (1 + 2) * 3 - 4 / 2\n"));
	EXPECT(0, MAIN("  val i: Int = 7 / 2\n  val d: Double = 7 / 2.0\n  val e: Double = 1 + 2.0\n"));
	EXPECT(0, MAIN("  val x = 5\n  val y: Int = -x\n  val z: Double = -1.5\n"));
	EXPECT(6, MAIN("  val i: Int = 1 + 2.0\n"));
	EXPECT(6, MAIN("  val i: Int = 7 / 2.0\n"));
	EXPECT(6, MAIN("  val x = () + 1\n"));
	EXPECT(6, MAIN("  val x = true + 1\n"));
	EXPECT(6, MAIN("  val x = -\"a\"\n"));
	EXPECT(6, MAIN("  val x = -true\n"));
	FINISH();
}

static void expressions_strings_3(void **state) {
	(void)state;
	EXPECT(0, MAIN("  val s: String = \"a\" + \"b\" + \"c\"\n"));
	// no automatic conversion to a string
	EXPECT(6, MAIN("  val s = \"a\" + 1\n"));
	EXPECT(6, MAIN("  val s = 1 + \"a\"\n"));
	EXPECT(6, MAIN("  val s = \"a\" + 1.5\n"));
	EXPECT(6, MAIN("  val s = \"a\" - \"b\"\n"));
	EXPECT(6, MAIN("  val s = \"a\" * 2\n"));
	EXPECT(6, MAIN("  val s = \"a\" / \"b\"\n"));
	FINISH();
}

static void expressions_relational_3(void **state) {
	(void)state;
	EXPECT(0, MAIN("  val a: Boolean = 1 < 2.0\n"
	               "  val b: Boolean = \"a\" <= \"b\"\n"
	               "  val c: Boolean = 1 == 1.0\n"
	               "  val d: Boolean = \"a\" != \"b\"\n"
	               "  val e: Boolean = true == false\n"));
	EXPECT(6, MAIN("  val x = 1 < \"a\"\n"));
	EXPECT(6, MAIN("  val x = true < false\n"));
	EXPECT(6, MAIN("  val x = \"a\" == 1\n"));
	EXPECT(6, MAIN("  val x = true != 1\n"));
	// chaining relational operators is not allowed, different priority levels are fine
	EXPECT(2, MAIN("  val x = 1 < 2 < 3\n"));
	EXPECT(2, MAIN("  val x = 1 == 2 == 3\n"));
	EXPECT(0, MAIN("  val x = 1 < 2 == true\n"));
	FINISH();
}

static void expressions_conditions_3(void **state) {
	(void)state;
	EXPECT(
	    0,
	    MAIN("  val x = 1\n  if x < 2 then\n    print(x)\n  else\n    ()\n  while x > 5 do\n    print(x)\n"));
	EXPECT(6, MAIN("  if 1 then\n    print(1)\n  else\n    ()\n"));
	EXPECT(6, MAIN("  while \"a\" do\n    print(1)\n"));
	EXPECT(6, MAIN("  if () then\n    print(1)\n"));
	EXPECT(6, MAIN("  val x = 1\n  while x do\n    print(x)\n"));
	FINISH();
}

// FUNEXP

static void funexp_calls_in_expressions_5_1(void **state) {
	(void)state;
	// the spec example
	EXPECT(0, WITH("def factorial(n: Int): Int =\n  if n <= 1 then 1 else n * factorial(n - 1)\n",
	               "  val result = 2 * factorial(readInt() + 1)\n"
	               "  print(toStr(result + strLength(subStr(\"abcd\", 1, 3))))\n"));
	EXPECT(0, MAIN("  if strLength(\"ab\") > 1 then print(1)\n"));
	EXPECT(0, WITH("def twice(x: Int): Int = x * 2\n", "  val n: Int = twice(twice(1) + twice(2))\n"));
	// argument types are checked inside expressions too
	EXPECT(5, MAIN("  val n = strLength(1 + 1)\n"));
	EXPECT(5, MAIN("  val n = 1 + strLength(2)\n"));
	EXPECT(6, MAIN("  val x = print(1) + 1\n"));
	EXPECT(6, MAIN("  val x: Int = strLength(\"a\") + 0.5\n"));
	EXPECT(3, MAIN("  val x = 1 + missing(2)\n"));
	FINISH();
}

// BOOLTHEN

static void boolthen_values_5_2(void **state) {
	(void)state;
	EXPECT(0, WITH("def negate(value: Boolean): Boolean = !value\n",
	               "  val a = true\n"
	               "  val b: Boolean = negate(a) && (1 < 2) || !a\n"
	               "  print(b)\n"
	               "  print(toStr(b == a))\n"));
	EXPECT(0, MAIN("  val ok = readBoolean()\n  if ok then\n    print(1)\n  else\n    ()\n  while !ok do\n   "
	               " print(2)\n"));
	EXPECT(6, MAIN("  val x = true && 1\n"));
	EXPECT(6, MAIN("  val x = 1 || 2\n"));
	EXPECT(6, MAIN("  val x = !1\n"));
	EXPECT(6, MAIN("  val x = !\"a\"\n"));
	FINISH();
}

static void boolthen_if_statements_5_2(void **state) {
	(void)state;
	EXPECT(0, MAIN("  val x = 1\n  if x < 0 then\n    print(\"negative\")\n"));
	EXPECT(0, MAIN("  val x = 1\n"
	               "  if x < 0 then\n"
	               "    print(\"negative\")\n"
	               "  else if x == 0 then\n"
	               "    print(\"zero\")\n"
	               "  else\n"
	               "    print(\"positive\")\n"));
	EXPECT(6, MAIN("  val x = 1\n  if x < 0 then\n    print(1)\n  else if x then\n    print(2)\n"));
	FINISH();
}

// CONTROL

static void control_if_expressions_5_3(void **state) {
	(void)state;
	// the spec examples
	EXPECT(0, MAIN("  var x = -5\n"
	               "  val absolute = if x < 0 then 0 - x else x\n"
	               "  val abs2 = if x < 0 then\n"
	               "    0 - x\n"
	               "  else\n"
	               "    x\n"
	               "  val abs3 = if x < 0 then 0 - x else\n"
	               "      x\n"
	               "  print(absolute + abs2 + abs3)\n"));
	EXPECT(0, WITH("def sign(x: Int): Int = if x < 0 then -1 else if x == 0 then 0 else 1\n",
	               "  print(sign(5))\n"));
	EXPECT(0, MAIN("  print(if true then 1 else 2)\n"));
	// both branches have the same type, Int can become Double
	EXPECT(0, MAIN("  val d: Double = if true then 1 else 2.0\n"));
	EXPECT(6, MAIN("  val i: Int = if true then 1 else 2.0\n"));
	EXPECT(6, MAIN("  val x = if true then 1 else \"a\"\n"));
	EXPECT(6, MAIN("  val x = if 1 then 1 else 2\n"));
	// else is required when if is an expression
	EXPECT(2, MAIN("  val x = 1\n  val y = if x < 0 then 1\n"));
	FINISH();
}

static void control_for_loops_5_3(void **state) {
	(void)state;
	EXPECT(0, MAIN("  for i <- 1 to 10 by 2 do\n    print(i)\n"));
	EXPECT(0, MAIN("  for i <- 0 until 3 do print(i)\n"));
	EXPECT(0, MAIN("  for i <- 6 to 2 by -2 do print(i)\n"));
	EXPECT(0, MAIN("  val limit = 10\n"
	               "  for\n"
	               "    i <- 0 until limit\n"
	               "    if i != 3\n"
	               "    if i < 8\n"
	               "  do\n"
	               "    print(i)\n"));
	EXPECT(0, MAIN("  for i <- 1 to 3 if i != 2 if i > 0 do print(i)\n"));
	EXPECT(0, MAIN("  for i <- 1 to 3 do\n    for j <- i to 3 do\n      print(i * j)\n"));
	// the loop variable is a new Int in its own scope
	EXPECT(0, MAIN("  val i = \"outer\"\n  for i <- 1 to 3 do print(i + 1)\n  print(i)\n"));
	EXPECT(6, MAIN("  for i <- 1 to 3 do\n    val s: String = i\n"));
	EXPECT(3, MAIN("  for i <- 1 to 3 do print(i)\n  print(i)\n"));
	EXPECT(7, MAIN("  for i <- 1 to 3 do i = 5\n"));
	FINISH();
}

static void control_for_ranges_5_3(void **state) {
	(void)state;
	// bounds and step are Int
	EXPECT(6, MAIN("  for i <- 1.5 to 3 do print(i)\n"));
	EXPECT(6, MAIN("  for i <- 1 to 3.0 do print(i)\n"));
	EXPECT(6, MAIN("  for i <- \"a\" until 3 do print(i)\n"));
	EXPECT(6, MAIN("  for i <- 1 to 3 by 1.0 do print(i)\n"));
	EXPECT(6, MAIN("  for i <- 1 to 3 by true do print(i)\n"));
	// a guard is Boolean
	EXPECT(6, MAIN("  for i <- 1 to 3 if i do print(i)\n"));
	// zero step: an error for a literal, a runtime check otherwise
	EXPECT(10, MAIN("  for i <- 1 to 3 by 0 do print(i)\n"));
	EXPECT(0, MAIN("  val step = 0\n  for i <- 1 to 3 by step do print(i)\n"));
	FINISH();
}

// VALMEM

static void valmem_members_5_4(void **state) {
	(void)state;
	EXPECT(0, MAIN("  val d = 1.5\n"
	               "  val a: Int = d.toInt\n"
	               "  val b: Double = d.floor\n"
	               "  val c: Int = d.floor.toInt\n"
	               "  val e: Int = \"abc\".length\n"
	               "  val f: String = a.toString\n"
	               "  val g: String = true.toString\n"
	               "  val u = ()\n"
	               "  val h: String = u.toString\n"));
	EXPECT(0, MAIN("  val n = toStr(12).length + 1\n  if \"ab\".length > 1 then print(n)\n"));
	EXPECT(0, MAIN("  val s: String = (1 + 2).toString\n"));
	EXPECT(6, MAIN("  val s: String = \"abc\".length\n"));
	EXPECT(6, MAIN("  val d = 1.5\n  val i: Int = d.floor\n"));
	EXPECT(6, MAIN("  val n: Int = 1.5.toString\n"));
	FINISH();
}

// PHASES TOGETHER

static void phases_first_error_kind_wins(void **state) {
	(void)state;
	// the whole program is checked only after parsing, so a later lexical or syntax error wins
	EXPECT(2, MAIN("  print(x)\n  val = 1\n"));
	EXPECT(1, MAIN("  print(x)\n  val s = \"unterminated\n"));
	EXPECT(2, WITH("def f(): Int = \"a\"\n", "  if then\n"));
	FINISH();
}

static void phases_programs_are_independent(void **state) {
	(void)state;
	// nothing from an earlier program, valid or not, may stay in the analyser
	EXPECT(6, MAIN("  val x: Int = \"a\"\n"));
	EXPECT(0, MAIN("  val x: Int = 1\n"));
	EXPECT(0, WITH("def helper(): Int = 1\n", "  print(helper())\n"));
	EXPECT(3, MAIN("  print(helper())\n"));
	EXPECT(2, MAIN("  val = 1\n"));
	EXPECT(0, MAIN("  val x = 1\n"));
	FINISH();
}

int main(void) {
	const struct CMUnitTest examples[] = {
	    cmocka_unit_test_setup(official_examples_are_valid, reset),
	    cmocka_unit_test_setup(official_examples_hand_over_ast, reset),
	};
	const struct CMUnitTest structure[] = {
	    cmocka_unit_test_setup(structure_entry_function_2, reset),
	    cmocka_unit_test_setup(structure_prolog_2, reset),
	    cmocka_unit_test_setup(variables_definition_and_types_2_2, reset),
	    cmocka_unit_test_setup(variables_assignment_2_2, reset),
	    cmocka_unit_test_setup(variables_scopes_2_2, reset),
	    cmocka_unit_test_setup(functions_definitions_2_3, reset),
	    cmocka_unit_test_setup(functions_return_types_2_3, reset),
	    cmocka_unit_test_setup(functions_body_result_2_3, reset),
	    cmocka_unit_test_setup(functions_calls_2_3, reset),
	};
	const struct CMUnitTest expressions[] = {
	    cmocka_unit_test_setup(builtins_valid_calls_4, reset),
	    cmocka_unit_test_setup(builtins_wrong_arguments_4, reset),
	    cmocka_unit_test_setup(builtins_result_types_4, reset),
	    cmocka_unit_test_setup(expressions_arithmetic_3, reset),
	    cmocka_unit_test_setup(expressions_strings_3, reset),
	    cmocka_unit_test_setup(expressions_relational_3, reset),
	    cmocka_unit_test_setup(expressions_conditions_3, reset),
	};
	const struct CMUnitTest extensions[] = {
	    cmocka_unit_test_setup(funexp_calls_in_expressions_5_1, reset),
	    cmocka_unit_test_setup(boolthen_values_5_2, reset),
	    cmocka_unit_test_setup(boolthen_if_statements_5_2, reset),
	    cmocka_unit_test_setup(control_if_expressions_5_3, reset),
	    cmocka_unit_test_setup(control_for_loops_5_3, reset),
	    cmocka_unit_test_setup(control_for_ranges_5_3, reset),
	    cmocka_unit_test_setup(valmem_members_5_4, reset),
	};
	const struct CMUnitTest phases[] = {
	    cmocka_unit_test_setup(phases_first_error_kind_wins, reset),
	    cmocka_unit_test_setup(phases_programs_are_independent, reset),
	};

	int failed = 0;
	failed += cmocka_run_group_tests_name("OFFICIAL EXAMPLES", examples, NULL, NULL);
	failed += cmocka_run_group_tests_name("STRUCTURE, VARIABLES, FUNCTIONS (2)", structure, NULL, NULL);
	failed += cmocka_run_group_tests_name("BUILT-INS AND EXPRESSIONS (3, 4)", expressions, NULL, NULL);
	failed += cmocka_run_group_tests_name("EXTENSIONS (5.1 to 5.4)", extensions, NULL, NULL);
	failed += cmocka_run_group_tests_name("PHASES TOGETHER", phases, NULL, NULL);
	sem_actions_free();
	return failed;
}
