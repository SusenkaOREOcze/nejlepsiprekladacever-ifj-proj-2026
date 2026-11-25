/**
	@file test_semantic_anal.c
	@author xmejstj00 (Kmejstr)
	@brief CMocka tests of the IFJ26 type rules (semantic_anal.c).
 */
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "cmocka/cmocka.h"

#include "../include/ast.h"
#include "../include/semantic_anal.h"
#include "../tests/ast_builder.h"

/*=======
  Fixture
=========*/

static ast_t *test_ast;
static funct_t *test_functions;

static int setup(void **state) {
	(void)state;
	test_ast = NULL;
	test_functions = NULL;
	return 0;
}

static int teardown(void **state) {
	(void)state;
	ast_destroy(&test_ast);
	if (test_functions != NULL)
		funct_free(test_functions);
	test_functions = NULL;
	return 0;
}

/**
 * Runs the semantic analysis of a program and returns its result.
 */
static sem_result_t analyze(ast_node_t *program) {
	assert_int_equal(ast_create(&test_ast), AST_RESULT_OK);
	assert_int_equal(ast_set_root(test_ast, program), AST_RESULT_OK);
	assert_int_equal(funct_init(&test_functions), SYMTSTAT_OK);

	return sem_analyze(test_ast, test_functions);
}

#define assert_semantic(expected, program) assert_int_equal(analyze(program), (expected))

/*=======================
  Program and @main rules
=========================*/

/* @main def main(): Unit = () */
static void minimal_program_is_valid(void **state) {
	(void)state;
	assert_semantic(SEM_OK, PROGRAM(MAIN(UNIT())));
}

/* def f(): Unit = () */
static void missing_main_is_error_3(void **state) {
	(void)state;
	assert_semantic(SEM_ERR_UNDEFINED, PROGRAM(FUNC("f", T_UNIT, UNIT())));
}

/* @main def main(): Unit = ()
   @main def second(): Unit = () */
static void two_mains_are_error_4(void **state) {
	(void)state;
	assert_semantic(SEM_ERR_REDEFINED, PROGRAM(MAIN(UNIT()), NODE(AST_NODE_MAIN_FUNCTION, "second", T_UNIT, UNIT())));
}

/* @main def main(n: Int): Unit = () */
static void main_with_parameter_is_error_5(void **state) {
	(void)state;
	assert_semantic(SEM_ERR_ARGUMENTS, PROGRAM(MAIN(PARAM("n", T_INT), UNIT())));
}

/* @main def main(): Int = 1 */
static void main_returning_int_is_error_6(void **state) {
	(void)state;
	assert_semantic(SEM_ERR_TYPE, PROGRAM(MAIN_T(T_INT, INT(1))));
}

/* @main def main() = print("x") */
static void main_with_inferred_unit_is_valid(void **state) {
	(void)state;
	assert_semantic(SEM_OK, PROGRAM(MAIN_T(T_NONE, CALL("print", STR("x")))));
}

/* def f(): Int = 1
   def f(a: Int): Unit = ()
   @main def main(): Unit = () */
static void function_redefinition_is_error_4(void **state) {
	(void)state;
	assert_semantic(SEM_ERR_REDEFINED, PROGRAM(FUNC("f", T_INT, INT(1)), FUNC("f", T_UNIT, PARAM("a", T_INT), UNIT()), MAIN(UNIT())));
}

/* def print(x: Int): Unit = () */
static void redefinition_of_builtin_is_error_4(void **state) {
	(void)state;
	assert_semantic(SEM_ERR_REDEFINED, PROGRAM(FUNC("print", T_UNIT, PARAM("x", T_INT), UNIT()), MAIN(UNIT())));
}

/* def f(a: Int, a: Int): Unit = () */
static void duplicate_parameter_is_error_4(void **state) {
	(void)state;
	assert_semantic(SEM_ERR_REDEFINED, PROGRAM(FUNC("f", T_UNIT, PARAM("a", T_INT), PARAM("a", T_INT), UNIT()), MAIN(UNIT())));
}

/*====================
  Variables and scopes
======================*/

/* print(x)
   val x = 1 */
static void use_before_definition_is_error_3(void **state) {
	(void)state;
	assert_semantic(SEM_ERR_UNDEFINED, IN_MAIN(CALL("print", ID("x")), VAL("x", INT(1))));
}

/* val x = x */
static void variable_is_not_visible_in_its_initializer(void **state) {
	(void)state;
	assert_semantic(SEM_ERR_UNDEFINED, IN_MAIN(VAL("x", ID("x"))));
}

/* val x = 1
   var x = 2 */
static void redefinition_in_same_block_is_error_4(void **state) {
	(void)state;
	assert_semantic(SEM_ERR_REDEFINED, IN_MAIN(VAL("x", INT(1)), VAR("x", INT(2))));
}

/* val x = 1
   while x < 2 do
     val x = "inner"
     print(x)
   print(x) */
static void shadowing_in_nested_block_is_valid(void **state) {
	(void)state;
	ast_node_t *outer = VAL("x", INT(1));
	ast_node_t *inner = VAL("x", STR("inner"));
	ast_node_t *inner_use = ID("x");
	ast_node_t *outer_use = ID("x");

	assert_semantic(SEM_OK, IN_MAIN(outer, WHILE(LT(ID("x"), INT(2)), BLOCK(inner, CALL("print", inner_use))), CALL("print", outer_use)));

	/* The two variables are different symbols and every use refers to the right one */
	assert_true(outer->symbol_id != AST_UNRESOLVED_SYMBOL);
	assert_true(inner->symbol_id != AST_UNRESOLVED_SYMBOL);
	assert_true(outer->symbol_id != inner->symbol_id);
	assert_true(inner_use->symbol_id == inner->symbol_id);
	assert_true(outer_use->symbol_id == outer->symbol_id);
	assert_int_equal(inner_use->data_type, T_STRING);
	assert_int_equal(outer_use->data_type, T_INT);
}

/* def f(n: Int): Int =
     val n = 2.5 -- a parameter may be shadowed in the body
     1 */
static void parameter_can_be_shadowed_in_body(void **state) {
	(void)state;
	assert_semantic(SEM_OK, PROGRAM(FUNC("f", T_INT, PARAM("n", T_INT), BLOCK(VAL("n", DBL(2.5)), INT(1))), MAIN(UNIT())));
}

/* if 1 < 2 then
     val y = 1
   else
     ()
   print(y) */
static void variable_is_not_visible_after_its_block(void **state) {
	(void)state;
	assert_semantic(SEM_ERR_UNDEFINED, IN_MAIN(IF(LT(INT(1), INT(2)), BLOCK(VAL("y", INT(1))), BLOCK(UNIT())), CALL("print", ID("y"))));
}

/* val x = 1
   x = 2 */
static void assignment_to_val_is_error_7(void **state) {
	(void)state;
	assert_semantic(SEM_ERR_IMMUTABLE, IN_MAIN(VAL("x", INT(1)), ASSIGN("x", INT(2))));
}

/* def f(n: Int): Unit =
     n = 2 */
static void assignment_to_parameter_is_error_7(void **state) {
	(void)state;
	assert_semantic(SEM_ERR_IMMUTABLE, PROGRAM(FUNC("f", T_UNIT, PARAM("n", T_INT), BLOCK(ASSIGN("n", INT(2)))), MAIN(UNIT())));
}

/* var x = 1
   x = "text" */
static void assignment_of_wrong_type_is_error_6(void **state) {
	(void)state;
	assert_semantic(SEM_ERR_TYPE, IN_MAIN(VAR("x", INT(1)), ASSIGN("x", STR("text"))));
}

/* var d = 1.5
   d = 2 */
static void assignment_converts_int_to_double(void **state) {
	(void)state;
	ast_node_t *assignment = ASSIGN("d", INT(2));
	assert_semantic(SEM_OK, IN_MAIN(VAR("d", DBL(1.5)), assignment));
	assert_int_equal(assignment->data_type, T_DOUBLE);
}

/* val a = 1.5
   val b: Double = 1
   val u = ()
   val w: Unit = print("x") */
static void declaration_types_are_inferred_and_converted(void **state) {
	(void)state;
	ast_node_t *a = VAL("a", DBL(1.5));
	ast_node_t *b = VAL_T("b", T_DOUBLE, INT(1));
	ast_node_t *u = VAL("u", UNIT());

	assert_semantic(SEM_OK, IN_MAIN(a, b, u, VAL_T("w", T_UNIT, CALL("print", STR("x")))));
	assert_int_equal(a->data_type, T_DOUBLE);
	assert_int_equal(b->data_type, T_DOUBLE);
	assert_int_equal(u->data_type, T_UNIT);
}

/* val x: Int = 1.5 */
static void declaration_does_not_convert_double_to_int(void **state) {
	(void)state;
	assert_semantic(SEM_ERR_TYPE, IN_MAIN(VAL_T("x", T_INT, DBL(1.5))));
}

/* The parser may also store the name of a declaration or assignment in an IDENTIFIER child:
   var x = 1
   x = 2 */
static void name_can_be_stored_in_identifier_child(void **state) {
	(void)state;
	ast_node_t *target = ID("x");
	assert_semantic(SEM_OK, IN_MAIN(NODE(AST_NODE_VAR_DECLARATION, NULL, T_NONE, ID("x"), INT(1)), NODE(AST_NODE_ASSIGNMENT, NULL, T_NONE, target, INT(2))));
	assert_true(target->symbol_id != AST_UNRESOLVED_SYMBOL);
	assert_int_equal(target->data_type, T_INT);
}

/*===========
  Expressions
=============*/

/* val x = 2
   val r = 1 + 2.5 * x */
static void arithmetic_types_are_written_into_the_ast(void **state) {
	(void)state;
	ast_node_t *product = MUL(DBL(2.5), ID("x"));
	ast_node_t *sum = ADD(INT(1), product);
	ast_node_t *result = VAL("r", sum);

	assert_semantic(SEM_OK, IN_MAIN(VAL("x", INT(2)), result));
	assert_int_equal(product->data_type, T_DOUBLE);
	assert_int_equal(sum->data_type, T_DOUBLE);
	assert_int_equal(result->data_type, T_DOUBLE);
	assert_int_equal(ast_node_child_at(sum, 0)->data_type, T_INT); /* needs a conversion to Double */
}

/* val a = 7 / 2
   val s = "a" + "b"
   val n = -a */
static void integer_division_concatenation_and_negation(void **state) {
	(void)state;
	ast_node_t *a = VAL("a", DIV(INT(7), INT(2)));
	ast_node_t *s = VAL("s", ADD(STR("a"), STR("b")));
	ast_node_t *n = VAL("n", NEG(ID("a")));

	assert_semantic(SEM_OK, IN_MAIN(a, s, n));
	assert_int_equal(a->data_type, T_INT);
	assert_int_equal(s->data_type, T_STRING);
	assert_int_equal(n->data_type, T_INT);
}

/* val s = "a" + 1 */
static void string_plus_int_is_error_6(void **state) {
	(void)state;
	assert_semantic(SEM_ERR_TYPE, IN_MAIN(VAL("s", ADD(STR("a"), INT(1)))));
}

/* val l = "a" ++ "b" -- ++ belongs to the unsupported extension LISTS */
static void list_concatenation_is_error_6(void **state) {
	(void)state;
	assert_semantic(SEM_ERR_TYPE, IN_MAIN(VAL("l", BINARY(AST_NODE_CONCATENATE, STR("a"), STR("b")))));
}

/* while "a" < "b" do ()
   while 1 < 2.5 do ()
   while () == () do () */
static void comparisons_of_compatible_types_are_valid(void **state) {
	(void)state;
	assert_semantic(SEM_OK, IN_MAIN(WHILE(LT(STR("a"), STR("b")), UNIT()), WHILE(LT(INT(1), DBL(2.5)), UNIT()), WHILE(EQ(UNIT(), UNIT()), UNIT())));
}

/* while "a" < 1 do () */
static void comparison_of_string_and_int_is_error_6(void **state) {
	(void)state;
	assert_semantic(SEM_ERR_TYPE, IN_MAIN(WHILE(LT(STR("a"), INT(1)), UNIT())));
}

/* while 1 do () */
static void condition_must_be_boolean(void **state) {
	(void)state;
	assert_semantic(SEM_ERR_TYPE, IN_MAIN(WHILE(INT(1), UNIT())));
}

/* val b = 1 < 2
   val c: Boolean = b == true
   print(c) */
static void boolean_is_a_value(void **state) {
	(void)state;
	ast_node_t *b = VAL("b", LT(INT(1), INT(2)));
	assert_semantic(SEM_OK, IN_MAIN(b, VAL_T("c", T_BOOL, EQ(ID("b"), BOOL(true))), CALL("print", ID("c"))));
	assert_int_equal(b->data_type, T_BOOL);
}

/* while !(1 < 2) || true && 2 > 1 do () */
static void logical_operators_are_valid(void **state) {
	(void)state;
	assert_semantic(SEM_OK, IN_MAIN(WHILE(OR(NOT(LT(INT(1), INT(2))), AND(BOOL(true), GT(INT(2), INT(1)))), UNIT())));
}

/* while 1 && 2 do () */
static void logical_and_of_ints_is_error_6(void **state) {
	(void)state;
	assert_semantic(SEM_ERR_TYPE, IN_MAIN(WHILE(AND(INT(1), INT(2)), UNIT())));
}

/* while true < false do () */
static void ordering_of_booleans_is_error_6(void **state) {
	(void)state;
	assert_semantic(SEM_ERR_TYPE, IN_MAIN(WHILE(LT(BOOL(true), BOOL(false)), UNIT())));
}

/* val v = if true then ()            -- else is mandatory in an if expression, even for Unit */
static void if_expression_without_else_is_error_even_for_unit(void **state) {
	(void)state;
	assert_semantic(SEM_ERR_TYPE, IN_MAIN(VAL("v", IF_NO_ELSE(BOOL(true), UNIT()))));
}

/*==============
  Function calls
================*/

/* print(1)
   print("a")
   print(())
   val n: Int = readInt()
   val d: Double = readInt()
   val i = toInt(1) -- Int is converted to the Double parameter
   val s = subStr("abcd", 1, 3)
   val c = chr(ord(s, 0)) -- nested calls need FUNEXP */
static void builtin_calls_are_valid(void **state) {
	(void)state;
	ast_node_t *s = VAL("s", CALL("subStr", STR("abcd"), INT(1), INT(3)));
	assert_semantic(SEM_OK, IN_MAIN(CALL("print", INT(1)), CALL("print", STR("a")), CALL("print", UNIT()), VAL_T("n", T_INT, CALL("readInt")),
		VAL_T("d", T_DOUBLE, CALL("readInt")), VAL("i", CALL("toInt", INT(1))), s, VAL("c", CALL("chr", CALL("ord", ID("s"), INT(0))))));
	assert_int_equal(s->data_type, T_STRING);
}

/* print() */
static void missing_argument_is_error_5(void **state) {
	(void)state;
	assert_semantic(SEM_ERR_ARGUMENTS, IN_MAIN(CALL("print")));
}

/* val s = subStr("abc", 1.5, 2) -- Double is not converted to Int */
static void double_argument_for_int_parameter_is_error_5(void **state) {
	(void)state;
	assert_semantic(SEM_ERR_ARGUMENTS, IN_MAIN(VAL("s", CALL("subStr", STR("abc"), DBL(1.5), INT(2)))));
}

/* val s: String = readInt() */
static void wrong_use_of_call_result_is_error_6(void **state) {
	(void)state;
	assert_semantic(SEM_ERR_TYPE, IN_MAIN(VAL_T("s", T_STRING, CALL("readInt"))));
}

/* missing() */
static void undefined_function_is_error_3(void **state) {
	(void)state;
	assert_semantic(SEM_ERR_UNDEFINED, IN_MAIN(CALL("missing")));
}

/* val b = readBoolean()
   print(b) */
static void read_boolean_returns_boolean(void **state) {
	(void)state;
	ast_node_t *b = VAL("b", CALL("readBoolean"));
	assert_semantic(SEM_OK, IN_MAIN(b, CALL("print", ID("b"))));
	assert_int_equal(b->data_type, T_BOOL);
}

/* def f(): Int = 1
   @main def main(): Unit =
     val x = f -- a function is not a variable */
static void function_name_is_not_a_variable(void **state) {
	(void)state;
	assert_semantic(SEM_ERR_UNDEFINED, PROGRAM(FUNC("f", T_INT, INT(1)), MAIN(BLOCK(VAL("x", ID("f"))))));
}

/* @main def main(): Unit =
     val r = scale(2, 3) -- called before its definition, Int converted to Double
     print(r)
   def scale(n: Int, factor: Double): Double = n * factor */
static void user_function_can_be_called_before_definition(void **state) {
	(void)state;
	ast_node_t *call = CALL("scale", INT(2), INT(3));
	assert_semantic(SEM_OK, PROGRAM(MAIN(BLOCK(VAL("r", call), CALL("print", ID("r")))), FUNC("scale", T_DOUBLE, PARAM("n", T_INT), PARAM("factor", T_DOUBLE), MUL(ID("n"), ID("factor")))));
	assert_int_equal(call->data_type, T_DOUBLE);

	/* The function table is filled for the code generator */
	funct_symbol_t symbol;
	assert_int_equal(funct_find_symbol(test_functions, "scale", &symbol), SYMTSTAT_OK);
	assert_int_equal(symbol.param_count, 2);
	assert_int_equal(symbol.ret_type, T_DOUBLE);
	assert_int_equal(sem_param_type(test_functions, "scale", 0), T_INT);
	assert_int_equal(sem_param_type(test_functions, "scale", 1), T_DOUBLE);
	assert_int_equal(sem_param_type(test_functions, "toInt", 0), T_DOUBLE);
	assert_int_equal(sem_param_type(test_functions, "print", 0), SEM_TYPE_ANY);
}

/* def f(a: Int, b: Double): Unit = ()
   @main def main(): Unit = f(1.5, 2) */
static void wrong_user_argument_type_is_error_5(void **state) {
	(void)state;
	assert_semantic(SEM_ERR_ARGUMENTS, PROGRAM(FUNC("f", T_UNIT, PARAM("a", T_INT), PARAM("b", T_DOUBLE), UNIT()), MAIN(CALL("f", DBL(1.5), INT(2)))));
}

/* print(toStr(2 * strLength(subStr("abcd", 1, 3)) + 1)) -- FUNEXP */
static void calls_inside_expressions_are_valid(void **state) {
	(void)state;
	assert_semantic(
	    SEM_OK,
	    IN_MAIN(CALL("print", CALL("toStr", ADD(MUL(INT(2), CALL("strLength", CALL("subStr", STR("abcd"), INT(1), INT(3)))), INT(1))))));
}

/*=======================
  Return types and values
=========================*/

/* The example program from the specification:
   def factorial(n: Int): Int =
     var result = 1
     var i: Int = n
     while i > 1 do
       result = result * i
       i = i - 1
     result
   @main def runFoo(): Unit =
     val n = readInt()
     val result = factorial(n)
     print(result)
     print("\n") */
static void specification_example_is_valid(void **state) {
	(void)state;
	assert_semantic(SEM_OK, PROGRAM(FUNC("factorial", T_INT, PARAM("n", T_INT),BLOCK(VAR("result", INT(1)), VAR_T("i", T_INT, ID("n")),WHILE(GT(ID("i"),
	INT(1)), BLOCK(ASSIGN("result", MUL(ID("result"), ID("i"))), ASSIGN("i", SUB(ID("i"), INT(1))))), ID("result"))), NODE(AST_NODE_MAIN_FUNCTION, "runFoo",
	T_UNIT, BLOCK(VAL("n", CALL("readInt")), VAL("result", CALL("factorial", ID("n"))), CALL("print", ID("result")), CALL("print", STR("\n"))))));
}

/* def f(): Int = "text" */
static void body_result_of_wrong_type_is_error_6(void **state) {
	(void)state;
	assert_semantic(SEM_ERR_TYPE, PROGRAM(FUNC("f", T_INT, STR("text")), MAIN(UNIT())));
}

/* def f(): Int =
     val x = 1 -- a block ending with a definition results in () */
static void body_ending_with_definition_results_in_unit(void **state) {
	(void)state;
	assert_semantic(SEM_ERR_TYPE, PROGRAM(FUNC("f", T_INT, BLOCK(VAL("x", INT(1)))), MAIN(UNIT())));
}

/* def f(): Unit =
     42 -- the result is thrown away in a Unit function */
static void unit_function_ignores_body_result(void **state) {
	(void)state;
	assert_semantic(SEM_OK, PROGRAM(FUNC("f", T_UNIT, BLOCK(INT(42))), MAIN(UNIT())));
}

/* def f(n: Int): Double =
     return n */
static void return_value_is_converted_to_return_type(void **state) {
	(void)state;
	ast_node_t *ret = RETURN(ID("n"));
	assert_semantic(SEM_OK, PROGRAM(FUNC("f", T_DOUBLE, PARAM("n", T_INT), BLOCK(ret)), MAIN(UNIT())));
	assert_int_equal(ret->data_type, T_DOUBLE);
}

/* def f(): Int =
     return */
static void return_without_value_in_int_function_is_error_6(void **state) {
	(void)state;
	assert_semantic(SEM_ERR_TYPE, PROGRAM(FUNC("f", T_INT, BLOCK(RETURN_NOTHING())), MAIN(UNIT())));
}

/* def f(): Unit =
     return
   def g(): Unit =
     return 5 -- evaluated and thrown away */
static void returns_in_unit_function_are_valid(void **state) {
	(void)state;
	assert_semantic(SEM_OK, PROGRAM(FUNC("f", T_UNIT, BLOCK(RETURN_NOTHING())), FUNC("g", T_UNIT, BLOCK(RETURN(INT(5)))), MAIN(UNIT())));
}

/* def f() =
     return 1 -- return needs an explicit return type */
static void return_without_explicit_return_type_is_error_6(void **state) {
	(void)state;
	assert_semantic(SEM_ERR_TYPE, PROGRAM(FUNC("f", T_NONE, BLOCK(RETURN(INT(1)))), MAIN(UNIT())));
}

/* def f(): Int =
     return 1
     print(missing) -- unreachable code is still checked */
static void unreachable_code_is_checked(void **state) {
	(void)state;
	assert_semantic(SEM_ERR_UNDEFINED, PROGRAM(FUNC("f", T_INT, BLOCK(RETURN(INT(1)), CALL("print", ID("missing")))), MAIN(UNIT())));
}

/*=====================
  Return type inference
=======================*/

/* def one() = 1
   @main def main(): Unit =
     val s: String = one() */
static void inferred_return_type_is_checked_at_call(void **state) {
	(void)state;
	assert_semantic(SEM_ERR_TYPE, PROGRAM(FUNC("one", T_NONE, INT(1)), MAIN(BLOCK(VAL_T("s", T_STRING, CALL("one"))))));
}

/* @main def main(): Unit =
     val x = a()
   def a() = b()
   def b() = c()
   def c() = 2.5 -- every type is inferred on demand, in any order */
static void return_types_are_inferred_through_a_chain(void **state) {
	(void)state;
	ast_node_t *x = VAL("x", CALL("a"));
	ast_node_t *a = FUNC("a", T_NONE, CALL("b"));
	assert_semantic(SEM_OK, PROGRAM(MAIN(BLOCK(x)), a, FUNC("b", T_NONE, CALL("c")), FUNC("c", T_NONE, DBL(2.5))));
	assert_int_equal(x->data_type, T_DOUBLE);
	assert_int_equal(a->data_type, T_DOUBLE);
}

/* @main def main(): Unit =
     val secret = 1
     val y = peek()
   def peek() = secret -- the variables of the caller are not visible in the callee */
static void callee_does_not_see_variables_of_caller(void **state) {
	(void)state;
	assert_semantic(SEM_ERR_UNDEFINED, PROGRAM(MAIN(BLOCK(VAL("secret", INT(1)), VAL("y", CALL("peek")))), FUNC("peek", T_NONE, ID("secret"))));
}

/* def f(n: Int) = f(n) */
static void direct_recursion_needs_explicit_return_type(void **state) {
	(void)state;
	assert_semantic(SEM_ERR_TYPE, PROGRAM(FUNC("f", T_NONE, PARAM("n", T_INT), CALL("f", ID("n"))), MAIN(UNIT())));
}

/* def f(n: Int) = g(n)
   def g(n: Int): Int = f(n) -- f is recursive through g, so f needs a type as well */
static void indirect_recursion_needs_explicit_return_type(void **state) {
	(void)state;
	assert_semantic(SEM_ERR_TYPE, PROGRAM(FUNC("f", T_NONE, PARAM("n", T_INT), CALL("g", ID("n"))), FUNC("g", T_INT, PARAM("n", T_INT), CALL("f", ID("n"))), MAIN(UNIT())));
}

/* def even(n: Int): Int = odd(n - 1)
   def odd(n: Int): Int = even(n - 1) */
static void recursion_with_explicit_return_types_is_valid(void **state) {
	(void)state;
	assert_semantic(SEM_OK, PROGRAM(FUNC("even", T_INT, PARAM("n", T_INT), CALL("odd", SUB(ID("n"), INT(1)))), FUNC("odd", T_INT, PARAM("n", T_INT), CALL("even", SUB(ID("n"), INT(1)))), MAIN(UNIT())));
}

/* def isSmall(n: Int) = n < 10
   @main def main(): Unit =
     val b: Boolean = isSmall(3) */
static void boolean_return_type_is_inferred(void **state) {
	(void)state;
	ast_node_t *is_small = FUNC("isSmall", T_NONE, PARAM("n", T_INT), LT(ID("n"), INT(10)));
	assert_semantic(SEM_OK, PROGRAM(is_small, MAIN(BLOCK(VAL_T("b", T_BOOL, CALL("isSmall", INT(3)))))));
	assert_int_equal(is_small->data_type, T_BOOL);
}

/*==============================
  if at the end of a function body
================================*/

/* def sign(n: Int): Int =
     if n > 0 then
       return 1
     else
       return 0 */
static ast_node_t *sign_with_returns(void) {
	return FUNC("sign", T_INT, PARAM("n", T_INT), BLOCK(IF(GT(ID("n"), INT(0)), BLOCK(RETURN(INT(1))), BLOCK(RETURN(INT(0))))));
}

static void body_may_end_with_if_whose_branches_return(void **state) {
	(void)state;
	assert_semantic(SEM_OK, PROGRAM(sign_with_returns(), MAIN(UNIT())));
}

/* def sign(n: Int): Int =
     if n > 0 then
       1
     else
       0 - the if is an expression (CONTROL) */
static ast_node_t *sign_with_values(symt_data_type_t ret_type) {
	return FUNC("sign", ret_type, PARAM("n", T_INT), BLOCK(IF(GT(ID("n"), INT(0)), BLOCK(INT(1)), BLOCK(INT(0)))));
}


static void if_produces_the_function_result(void **state) {
	(void)state;
	ast_node_t *sign = sign_with_values(T_NONE);
	assert_semantic(SEM_OK, PROGRAM(sign, MAIN(UNIT())));
	assert_int_equal(sign->data_type, T_INT);
}

/* def show(n: Int) =
     if n > 0 then
       print("positive")
     else
       print("other") -- all branches result in Unit, so Unit is inferred */
static void unit_is_inferred_from_if_with_unit_branches(void **state) {
	(void)state;
	ast_node_t *show = FUNC("show", T_NONE, PARAM("n", T_INT), BLOCK(IF(GT(ID("n"), INT(0)), BLOCK(CALL("print", STR("positive"))), BLOCK(CALL("print", STR("other"))))));
	assert_semantic(SEM_OK, PROGRAM(show, MAIN(UNIT())));
	assert_int_equal(show->data_type, T_UNIT);
}

/* def f(n: Int): Int =
     if n > 0 then
       return 1
     else
       0 -- the branch with return produces no value */
static ast_node_t *mixed_return_and_value(void) {
	return FUNC("f", T_INT, PARAM("n", T_INT), BLOCK(IF(GT(ID("n"), INT(0)), BLOCK(RETURN(INT(1))), BLOCK(INT(0)))));
}

static void if_mixing_return_and_value(void **state) {
	(void)state;
	assert_semantic(SEM_OK, PROGRAM(mixed_return_and_value(), MAIN(UNIT())));
}

/* def f(n: Int): Int =
     if n > 0 then
       1 -- the missing else behaves like a branch with () */
static void if_without_else_cannot_produce_int(void **state) {
	(void)state;
	assert_semantic(SEM_ERR_TYPE, PROGRAM(FUNC("f", T_INT, PARAM("n", T_INT), BLOCK(IF_NO_ELSE(GT(ID("n"), INT(0)), BLOCK(INT(1))))), MAIN(UNIT())));
}

/*=====================================
  BOOLTHEN and CONTROL: if without else
  and if as an expression
=======================================*/

/* val x = readInt()
   if x < 0 then
     print("negative")
   else if x == 0 then
     print("zero")
   print("done") */
static void if_without_else_and_else_if_are_valid(void **state) {
	(void)state;
	assert_semantic(SEM_OK, IN_MAIN(VAL("x", CALL("readInt")), IF(LT(ID("x"), INT(0)), BLOCK(CALL("print", STR("negative"))),
	IF_NO_ELSE(EQ(ID("x"), INT(0)), BLOCK(CALL("print", STR("zero"))))), CALL("print", STR("done"))));
}

/* val x = readInt()
   val absolute = if x < 0 then 0 - x else x
   val mixed = if x < 0 then 1 else 2.5 */
static void if_expression_has_the_type_of_its_branches(void **state) {
	(void)state;
	ast_node_t *absolute = VAL("absolute", IF(LT(ID("x"), INT(0)), SUB(INT(0), ID("x")), ID("x")));
	ast_node_t *mixed_if = IF(LT(ID("x"), INT(0)), INT(1), DBL(2.5));
	ast_node_t *mixed = VAL("mixed", mixed_if);

	assert_semantic(SEM_OK, IN_MAIN(VAL("x", CALL("readInt")), absolute, mixed));
	assert_int_equal(absolute->data_type, T_INT);
	assert_int_equal(mixed_if->data_type, T_DOUBLE);
	assert_int_equal(mixed->data_type, T_DOUBLE);
}

/* val v = if 1 < 2 then 1 else "two" */
static void if_expression_with_different_types_is_error_6(void **state) {
	(void)state;
	assert_semantic(SEM_ERR_TYPE, IN_MAIN(VAL("v", IF(LT(INT(1), INT(2)), INT(1), STR("two")))));
}

/* val v: Int = if 1 < 2 then 1 -- else is mandatory in an if expression */
static void if_expression_without_else_is_error_6(void **state) {
	(void)state;
	assert_semantic(SEM_ERR_TYPE, IN_MAIN(VAL_T("v", T_INT, IF_NO_ELSE(LT(INT(1), INT(2)), INT(1)))));
}

/*============
  CONTROL: for
==============*/

/* for i <- 1 to 10 by 2 do
     print(i) */
static void for_loop_is_valid(void **state) {
	(void)state;
	ast_node_t *generator = GENERATOR("i", TO_BY(INT(1), INT(10), INT(2)));
	ast_node_t *use = ID("i");

	assert_semantic(SEM_OK, IN_MAIN(FOR(generator, BLOCK(CALL("print", use)))));
	assert_int_equal(generator->data_type, T_INT);
	assert_true(generator->symbol_id != AST_UNRESOLVED_SYMBOL);
	assert_true(use->symbol_id == generator->symbol_id);
}

/* val limit = 10
   for
     i <- 0 until limit
     if i != 3
     if i < 8
   do
     val i = "shadow"
     print(i) */
static void for_loop_with_guards_is_valid(void **state) {
	(void)state;
	assert_semantic(SEM_OK, IN_MAIN(VAL("limit", INT(10)), FOR(GENERATOR("i", UNTIL(INT(0), ID("limit"))), NE(ID("i"), INT(3)),
	LT(ID("i"), INT(8)), BLOCK(VAL("i", STR("shadow")), CALL("print", ID("i"))))));
}

/* for i <- 1 to 2.5 do () */
static void for_bound_must_be_int(void **state) {
	(void)state;
	assert_semantic(SEM_ERR_TYPE, IN_MAIN(FOR(GENERATOR("i", TO(INT(1), DBL(2.5))), UNIT())));
}

/* for i <- 1 to 9 by 0 do () */
static void for_zero_step_is_error_10(void **state) {
	(void)state;
	assert_semantic(SEM_ERR_OTHER, IN_MAIN(FOR(GENERATOR("i", TO_BY(INT(1), INT(9), INT(0))), UNIT())));
}

/* for i <- 1 to 9 do
     i = 5 */
static void for_variable_is_immutable(void **state) {
	(void)state;
	assert_semantic(SEM_ERR_IMMUTABLE, IN_MAIN(FOR(GENERATOR("i", TO(INT(1), INT(9))), BLOCK(ASSIGN("i", INT(5))))));
}

/* for i <- 1 to 9 do ()
   print(i) */
static void for_variable_is_not_visible_after_loop(void **state) {
	(void)state;
	assert_semantic(SEM_ERR_UNDEFINED, IN_MAIN(FOR(GENERATOR("i", TO(INT(1), INT(9))), UNIT()), CALL("print", ID("i"))));
}

/*==============
  Malformed AST
================*/

/* A node that is neither a statement nor an expression inside a body */
static void malformed_ast_is_error_99(void **state) {
	(void)state;
	assert_semantic(SEM_ERR_INTERNAL, IN_MAIN(PARAM("stray", T_INT), UNIT()));
}

/** Registers one test with the common fixture (a fresh AST and function table). */
#define SEM_TEST(test) cmocka_unit_test_setup_teardown(test, setup, teardown)

/**Runs one named group of tests and remembers whether any of them failed.*/
#define RUN_GROUP(name, tests) (failed |= cmocka_run_group_tests_name((name), (tests), NULL, NULL))

int main(void) {
	const struct CMUnitTest program_tests[] = {
	    SEM_TEST(minimal_program_is_valid),
	    SEM_TEST(missing_main_is_error_3),
	    SEM_TEST(two_mains_are_error_4),
	    SEM_TEST(main_with_parameter_is_error_5),
	    SEM_TEST(main_returning_int_is_error_6),
	    SEM_TEST(main_with_inferred_unit_is_valid),
	    SEM_TEST(function_redefinition_is_error_4),
	    SEM_TEST(redefinition_of_builtin_is_error_4),
	    SEM_TEST(duplicate_parameter_is_error_4),
	};

	const struct CMUnitTest variable_tests[] = {
	    SEM_TEST(use_before_definition_is_error_3),
	    SEM_TEST(variable_is_not_visible_in_its_initializer),
	    SEM_TEST(redefinition_in_same_block_is_error_4),
	    SEM_TEST(shadowing_in_nested_block_is_valid),
	    SEM_TEST(parameter_can_be_shadowed_in_body),
	    SEM_TEST(variable_is_not_visible_after_its_block),
	    SEM_TEST(assignment_to_val_is_error_7),
	    SEM_TEST(assignment_to_parameter_is_error_7),
	    SEM_TEST(assignment_of_wrong_type_is_error_6),
	    SEM_TEST(assignment_converts_int_to_double),
	    SEM_TEST(declaration_types_are_inferred_and_converted),
	    SEM_TEST(declaration_does_not_convert_double_to_int),
	    SEM_TEST(name_can_be_stored_in_identifier_child),
	};

	const struct CMUnitTest expression_tests[] = {
	    SEM_TEST(arithmetic_types_are_written_into_the_ast),
	    SEM_TEST(integer_division_concatenation_and_negation),
	    SEM_TEST(string_plus_int_is_error_6),
	    SEM_TEST(list_concatenation_is_error_6),
	    SEM_TEST(comparisons_of_compatible_types_are_valid),
	    SEM_TEST(comparison_of_string_and_int_is_error_6),
	    SEM_TEST(condition_must_be_boolean),
	    SEM_TEST(boolean_is_a_value),
	    SEM_TEST(logical_operators_are_valid),
	    SEM_TEST(logical_and_of_ints_is_error_6),
	    SEM_TEST(ordering_of_booleans_is_error_6),
		SEM_TEST(if_expression_without_else_is_error_even_for_unit),
	};

	const struct CMUnitTest call_tests[] = {
	    SEM_TEST(builtin_calls_are_valid),
	    SEM_TEST(missing_argument_is_error_5),
	    SEM_TEST(double_argument_for_int_parameter_is_error_5),
	    SEM_TEST(wrong_use_of_call_result_is_error_6),
	    SEM_TEST(undefined_function_is_error_3),
	    SEM_TEST(read_boolean_returns_boolean),
	    SEM_TEST(function_name_is_not_a_variable),
	    SEM_TEST(user_function_can_be_called_before_definition),
	    SEM_TEST(wrong_user_argument_type_is_error_5),
	    SEM_TEST(calls_inside_expressions_are_valid),
	};

	const struct CMUnitTest return_tests[] = {
	    SEM_TEST(specification_example_is_valid),
	    SEM_TEST(body_result_of_wrong_type_is_error_6),
	    SEM_TEST(body_ending_with_definition_results_in_unit),
	    SEM_TEST(unit_function_ignores_body_result),
	    SEM_TEST(return_value_is_converted_to_return_type),
	    SEM_TEST(return_without_value_in_int_function_is_error_6),
	    SEM_TEST(returns_in_unit_function_are_valid),
	    SEM_TEST(return_without_explicit_return_type_is_error_6),
	    SEM_TEST(unreachable_code_is_checked),
	};

	const struct CMUnitTest inference_tests[] = {
	    SEM_TEST(inferred_return_type_is_checked_at_call),
	    SEM_TEST(return_types_are_inferred_through_a_chain),
	    SEM_TEST(callee_does_not_see_variables_of_caller),
	    SEM_TEST(direct_recursion_needs_explicit_return_type),
	    SEM_TEST(indirect_recursion_needs_explicit_return_type),
	    SEM_TEST(recursion_with_explicit_return_types_is_valid),
	    SEM_TEST(boolean_return_type_is_inferred),
	};

	const struct CMUnitTest body_if_tests[] = {
	    SEM_TEST(body_may_end_with_if_whose_branches_return),
	    SEM_TEST(if_produces_the_function_result),
	    SEM_TEST(unit_is_inferred_from_if_with_unit_branches),
	    SEM_TEST(if_mixing_return_and_value),
	    SEM_TEST(if_without_else_cannot_produce_int),
	};

	const struct CMUnitTest if_expression_tests[] = {
	    SEM_TEST(if_without_else_and_else_if_are_valid),
	    SEM_TEST(if_expression_has_the_type_of_its_branches),
	    SEM_TEST(if_expression_with_different_types_is_error_6),
	    SEM_TEST(if_expression_without_else_is_error_6),
	};

	const struct CMUnitTest for_tests[] = {
	    SEM_TEST(for_loop_is_valid),
	    SEM_TEST(for_loop_with_guards_is_valid),
	    SEM_TEST(for_bound_must_be_int),
	    SEM_TEST(for_zero_step_is_error_10),
	    SEM_TEST(for_variable_is_immutable),
	    SEM_TEST(for_variable_is_not_visible_after_loop),
	};

	const struct CMUnitTest malformed_tests[] = {
	    SEM_TEST(malformed_ast_is_error_99),
	};

	int failed = 0;

	RUN_GROUP("Program and @main", program_tests);
	RUN_GROUP("Variables and scopes", variable_tests);
	RUN_GROUP("Expressions", expression_tests);
	RUN_GROUP("Function calls", call_tests);
	RUN_GROUP("Return types and values", return_tests);
	RUN_GROUP("Return type inference", inference_tests);
	RUN_GROUP("if at the end of a function body", body_if_tests);
	RUN_GROUP("BOOLTHEN and CONTROL: if", if_expression_tests);
	RUN_GROUP("CONTROL: for", for_tests);
	RUN_GROUP("Malformed AST", malformed_tests);

	return failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}