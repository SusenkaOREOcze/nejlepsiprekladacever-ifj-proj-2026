/**
	@file test_sem_types.c
	@author xmejstj00 (Kmejstr)
	@brief CMocka tests of the IFJ26 type rules (sem_types.c).
 */

#include <stddef.h>
#include <stdint.h>

#include "cmocka/cmocka.h"

#include "../include/sem_types.h"

#define I SYMT_TYPE_INTEGER
#define D SYMT_TYPE_DOUBLE
#define S SYMT_TYPE_STRING
#define U SYMT_TYPE_UNIT	
#define B SYMT_TYPE_BOOLEAN
#define X SYMT_TYPE_UNDEFINED /* Not allowed -> error 6, we do not accept this kind around these corners */

static void arithmetic_operators(void **state) {
	(void)state;
	assert_int_equal(sem_binary_type(AST_NODE_ADD, I, I), I);
	assert_int_equal(sem_binary_type(AST_NODE_ADD, I, D), D); /* Int is widened */
	assert_int_equal(sem_binary_type(AST_NODE_SUBTRACT, D, I), D);
	assert_int_equal(sem_binary_type(AST_NODE_MULTIPLY, D, D), D);
	assert_int_equal(sem_binary_type(AST_NODE_DIVIDE, I, I), I); /* Integer division */

	assert_int_equal(sem_binary_type(AST_NODE_ADD, S, S), S); /* Concatenation */
	assert_int_equal(sem_binary_type(AST_NODE_ADD, S, I), X); /* No conversion to String */
	assert_int_equal(sem_binary_type(AST_NODE_MULTIPLY, S, S), X);
	assert_int_equal(sem_binary_type(AST_NODE_SUBTRACT, U, U), X);
	assert_int_equal(sem_binary_type(AST_NODE_ADD, B, B), X);
	assert_int_equal(sem_binary_type(AST_NODE_ADD, I, X), X);
	assert_int_equal(sem_binary_type(AST_NODE_CONCATENATE, S, S), X); /* ++ belongs to LISTS */
}

static void relational_operators(void **state) {
	(void)state;
	assert_int_equal(sem_binary_type(AST_NODE_LESS, I, D), B);
	assert_int_equal(sem_binary_type(AST_NODE_GREATER_EQUAL, S, S), B);
	assert_int_equal(sem_binary_type(AST_NODE_LESS, S, I), X);
	assert_int_equal(sem_binary_type(AST_NODE_LESS_EQUAL, U, U), X);
	assert_int_equal(sem_binary_type(AST_NODE_LESS, B, B), X); /* Booleans have no ordering */

	assert_int_equal(sem_binary_type(AST_NODE_EQUAL, I, D), B);
	assert_int_equal(sem_binary_type(AST_NODE_EQUAL, U, U), B);
	assert_int_equal(sem_binary_type(AST_NODE_EQUAL, B, B), B);
	assert_int_equal(sem_binary_type(AST_NODE_NOT_EQUAL, S, S), B);
	assert_int_equal(sem_binary_type(AST_NODE_NOT_EQUAL, S, I), X);
}

static void logical_operators(void **state) {
	(void)state;
	assert_int_equal(sem_binary_type(AST_NODE_AND, B, B), B);
	assert_int_equal(sem_binary_type(AST_NODE_OR, B, B), B);
	assert_int_equal(sem_binary_type(AST_NODE_OR, B, I), X);
	assert_int_equal(sem_binary_type(AST_NODE_AND, I, I), X);
	assert_int_equal(sem_unary_type(AST_NODE_NOT, B), B);
	assert_int_equal(sem_unary_type(AST_NODE_NOT, I), X);
}

static void unary_sign_operators(void **state) {
	(void)state;
	assert_int_equal(sem_unary_type(AST_NODE_NEGATIVE, I), I);
	assert_int_equal(sem_unary_type(AST_NODE_NEGATIVE, D), D);
	assert_int_equal(sem_unary_type(AST_NODE_POSITIVE, I), I);
	assert_int_equal(sem_unary_type(AST_NODE_NEGATIVE, S), X);
	assert_int_equal(sem_unary_type(AST_NODE_NEGATIVE, B), X);
}

static void assignability(void **state) {
	(void)state;
	assert_true(sem_assignable(I, I));
	assert_true(sem_assignable(D, I)); /* val x: Double = 1 */
	assert_true(sem_assignable(U, U));
	assert_true(sem_assignable(B, B));
	assert_false(sem_assignable(I, D)); /* Never the other way round */
	assert_false(sem_assignable(S, I));
	assert_false(sem_assignable(I, X));
	assert_false(sem_assignable(X, X));
}

static void unification(void **state) {
	(void)state;
	assert_int_equal(sem_unify(I, I), I);
	assert_int_equal(sem_unify(I, D), D);
	assert_int_equal(sem_unify(D, I), D);
	assert_int_equal(sem_unify(S, S), S);
	assert_int_equal(sem_unify(B, B), B);
	assert_int_equal(sem_unify(S, I), X);
	assert_int_equal(sem_unify(U, I), X);
}

static void value_types(void **state) {
	(void)state;
	assert_true(sem_is_value_type(I));
	assert_true(sem_is_value_type(U));
	assert_true(sem_is_value_type(B));
	assert_false(sem_is_value_type(X));
}

int main(void) {
	const struct CMUnitTest tests[] = {
	    cmocka_unit_test(arithmetic_operators),
	    cmocka_unit_test(relational_operators),
	    cmocka_unit_test(logical_operators),
	    cmocka_unit_test(unary_sign_operators),
	    cmocka_unit_test(assignability),
	    cmocka_unit_test(unification),
	    cmocka_unit_test(value_types),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
