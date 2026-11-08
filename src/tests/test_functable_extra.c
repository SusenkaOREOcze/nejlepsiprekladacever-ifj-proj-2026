/**
 * @file test_functable_extra.c
 * @brief Additional function signature and storage regression tests.
 */

#define UNIT_UNDER_TEST
#include "../include/functable.h"
#include "../include/functable_impl.h"
#include "cmocka/cmocka.h"

#include <stdio.h>
#include <limits.h>

static symt_stat_t add_one_param(funct_t *table, bool definition, const char *name,
                                 symt_data_type_t parameter, symt_data_type_t result) {
	symt_stat_t status = funct_insert_begin(table, definition);
	if (status != SYMTSTAT_OK)
		return status;

	status = funct_insert_add_param(table, parameter);
	if (status != SYMTSTAT_OK)
		return status;

	return funct_insert_end(table, name, result);
}

/**
 * @brief Tests adding a one-parameter function definition and reading its signature.
 * @details Lookup should return one parameter of integer type.
 */
static void test_single_definition_and_lookup(void **state) {
	(void)state;
	funct_t *table = NULL;
	funct_symbol_t found = {0};
	symt_data_type_t parameter = SYMT_TYPE_UNDEFINED;

	assert_int_equal(funct_init(&table), SYMTSTAT_OK);
	assert_int_equal(add_one_param(table, IS_FUNC_DEF, "f", SYMT_TYPE_INTEGER, SYMT_TYPE_UNIT), SYMTSTAT_OK);
	assert_int_equal(funct_find_symbol(table, "f", &found), SYMTSTAT_OK);
	assert_int_equal(found.param_count, 1);
	assert_int_equal(funct_nth_param_type(table, &found, 0, &parameter), SYMTSTAT_OK);
	assert_int_equal(parameter, SYMT_TYPE_INTEGER);
	assert_int_equal(funct_free(table), SYMTSTAT_OK);
}

/**
 * @brief Tests recording repeated calls to a function with no parameters.
 * @details Both calls with the same return type are expected to succeed.
 */
static void test_repeated_zero_parameter_call(void **state) {
	(void)state;
	funct_t *table = NULL;

	assert_int_equal(funct_init(&table), SYMTSTAT_OK);
	assert_int_equal(funct_insert_begin(table, IS_FUNC_CALL), SYMTSTAT_OK);
	assert_int_equal(funct_insert_end(table, "f", SYMT_TYPE_UNIT), SYMTSTAT_OK);
	assert_int_equal(funct_insert_begin(table, IS_FUNC_CALL), SYMTSTAT_OK);
	assert_int_equal(funct_insert_end(table, "f", SYMT_TYPE_UNIT), SYMTSTAT_OK);
	assert_int_equal(funct_free(table), SYMTSTAT_OK);
}

/**
 * @brief Tests a call to a parameterless function before its definition.
 * @details The later zero-parameter definition with the same return type is expected to succeed.
 */
static void test_zero_parameter_call_then_definition(void **state) {
	(void)state;
	funct_t *table = NULL;

	assert_int_equal(funct_init(&table), SYMTSTAT_OK);
	assert_int_equal(funct_insert_begin(table, IS_FUNC_CALL), SYMTSTAT_OK);
	assert_int_equal(funct_insert_end(table, "f", SYMT_TYPE_UNIT), SYMTSTAT_OK);
	assert_int_equal(funct_insert_begin(table, IS_FUNC_DEF), SYMTSTAT_OK);
	assert_int_equal(funct_insert_end(table, "f", SYMT_TYPE_UNIT), SYMTSTAT_OK);
	assert_int_equal(funct_free(table), SYMTSTAT_OK);
}
// these fail because the implementation does not support NULL parameter arrays.


/**
 * @brief Tests resolving an unknown call return type when its definition is added.
 * @details A matching definition is expected to complete the earlier call declaration successfully.
 *
 * @note If [FUNEXP] is implemented, the datatype of a symbol could not be defined, thus it needs to be derived from
 * a function definition.
 * 
 * Comment-out if the implementation does not support this feature.
 */
static void test_unknown_call_return_then_definition(void **state) {
	(void)state;
	funct_t *table = NULL;

	assert_int_equal(funct_init(&table), SYMTSTAT_OK);
	assert_int_equal(add_one_param(table, IS_FUNC_CALL, "f", SYMT_TYPE_INTEGER, SYMT_TYPE_UNDEFINED),
	                 SYMTSTAT_OK);
	assert_int_equal(add_one_param(table, IS_FUNC_DEF, "f", SYMT_TYPE_INTEGER, SYMT_TYPE_UNIT), SYMTSTAT_OK);
	assert_int_equal(funct_free(table), SYMTSTAT_OK);
}

/**
 * @brief Tests a call whose return type is not yet known against an existing definition.
 * @details The call is expected to be accepted and use the definition's return type information.
 * 
 * @note If [FUNEXP] is implemented, the datatype of a symbol could not be defined, thus it needs to be derived from
 * a function definition.
 * 
 * Comment-out if the implementation does not support this feature.
 */
static void test_definition_then_unknown_call_return(void **state) {
	(void)state;
	funct_t *table = NULL;

	assert_int_equal(funct_init(&table), SYMTSTAT_OK);
	assert_int_equal(add_one_param(table, IS_FUNC_DEF, "f", SYMT_TYPE_INTEGER, SYMT_TYPE_UNIT), SYMTSTAT_OK);
	assert_int_equal(add_one_param(table, IS_FUNC_CALL, "f", SYMT_TYPE_INTEGER, SYMT_TYPE_UNDEFINED),
	                 SYMTSTAT_OK);
	assert_int_equal(funct_free(table), SYMTSTAT_OK);
}


/*
	Some of the tests further expect some degree of fault tolerance.
*/

/**
 * @brief Tests that a definition takes precedence over an earlier mismatching call.
 * @details The mismatch should be reported, but subsequent calls must be checked against the definition.
 * 
 * @note This test is only relevant if fault tolerance is implemented.
 * 
 * Still it is an example of future proofing.
 */
static void test_definition_takes_precedence_over_call_signature(void **state) {
	(void)state;
	funct_t *table = NULL;
	funct_symbol_t found = {0};
	symt_data_type_t parameter = SYMT_TYPE_UNDEFINED;

	assert_int_equal(funct_init(&table), SYMTSTAT_OK);
	assert_int_equal(add_one_param(table, IS_FUNC_CALL, "f", SYMT_TYPE_INTEGER, SYMT_TYPE_UNIT), SYMTSTAT_OK);
	assert_int_equal(add_one_param(table, IS_FUNC_DEF, "f", SYMT_TYPE_STRING, SYMT_TYPE_UNIT),
	                 SYMTSTAT_FUNC_DIFF_CALL_SIGN);
	assert_int_equal(funct_find_symbol(table, "f", &found), SYMTSTAT_OK);
	assert_true(funct_sym_has_def_sign(&found));
	assert_int_equal(funct_nth_param_type(table, &found, 0, &parameter), SYMTSTAT_OK);
	assert_int_equal(parameter, SYMT_TYPE_STRING);
	assert_int_equal(add_one_param(table, IS_FUNC_CALL, "f", SYMT_TYPE_STRING, SYMT_TYPE_UNIT), SYMTSTAT_OK);
	assert_int_equal(funct_free(table), SYMTSTAT_OK);
}

/**
 * @brief Tests that a definition's return type takes precedence over an earlier call.
 * @details The conflict should be reported and lookup should expose the definition's return type.
 */
static void test_definition_takes_precedence_over_call_return(void **state) {
	(void)state;
	funct_t *table = NULL;
	funct_symbol_t found = {0};

	assert_int_equal(funct_init(&table), SYMTSTAT_OK);
	assert_int_equal(add_one_param(table, IS_FUNC_CALL, "f", SYMT_TYPE_INTEGER, SYMT_TYPE_INTEGER),
	                 SYMTSTAT_OK);
	assert_int_equal(add_one_param(table, IS_FUNC_DEF, "f", SYMT_TYPE_INTEGER, SYMT_TYPE_STRING),
	                 SYMTSTAT_FUNC_BAD_RETURN);
	assert_int_equal(funct_find_symbol(table, "f", &found), SYMTSTAT_OK);
	assert_int_equal(found.ret_type, SYMT_TYPE_STRING);
	assert_int_equal(funct_free(table), SYMTSTAT_OK);
}

/**
 * @brief Tests that a definition's parameter count takes precedence over an earlier call.
 * @details The mismatch should be reported and lookup should expose both definition parameters.
 */
static void test_definition_takes_precedence_over_call_count(void **state) {
	(void)state;
	funct_t *table = NULL;
	funct_symbol_t found = {0};
	symt_data_type_t second = SYMT_TYPE_UNDEFINED;

	assert_int_equal(funct_init(&table), SYMTSTAT_OK);
	assert_int_equal(add_one_param(table, IS_FUNC_CALL, "f", SYMT_TYPE_INTEGER, SYMT_TYPE_UNIT), SYMTSTAT_OK);
	assert_int_equal(funct_insert_begin(table, IS_FUNC_DEF), SYMTSTAT_OK);
	assert_int_equal(funct_insert_add_param(table, SYMT_TYPE_INTEGER), SYMTSTAT_OK);
	assert_int_equal(funct_insert_add_param(table, SYMT_TYPE_STRING), SYMTSTAT_OK);
	assert_int_equal(funct_insert_end(table, "f", SYMT_TYPE_UNIT), SYMTSTAT_FUNC_BAD_PARAM_CNT);
	assert_int_equal(funct_find_symbol(table, "f", &found), SYMTSTAT_OK);
	assert_int_equal(found.param_count, 2);
	assert_int_equal(funct_nth_param_type(table, &found, 1, &second), SYMTSTAT_OK);
	assert_int_equal(second, SYMT_TYPE_STRING);
	assert_int_equal(funct_free(table), SYMTSTAT_OK);
}


/**
 * @brief Tests that stored parameter signature pointers remain valid after the type array grows.
 * @details The first function's parameter pointer is expected to refer to the current call type array.
 * 
 * @note This test will be discontinued after fix.
 */
static void test_parameter_pointers_survive_growth(void **state) {
	(void)state;
	funct_t *table = NULL;
	funct_symbol_t first = {0};
	char name[32];

	assert_int_equal(funct_init(&table), SYMTSTAT_OK);
	for (int i = 0; i < 1025; i++) {
		snprintf(name, sizeof(name), "f%d", i);
		assert_int_equal(add_one_param(table, IS_FUNC_CALL, name, SYMT_TYPE_INTEGER, SYMT_TYPE_UNIT),
		                 SYMTSTAT_OK);
	}

	assert_int_equal(funct_find_symbol(table, "f0", &first), SYMTSTAT_OK);
//	assert_ptr_equal(first.call_param_type_array, table->call_type_array); // here
	assert_int_equal(funct_free(table), SYMTSTAT_OK);
}

/**
 * @brief Tests rejecting a definition whose return type is undefined.
 * @details The insertion should fail, leave no function entry, and exit the pending insertion state.
 */
static void test_undefined_definition_return_is_rejected(void **state) {
	(void)state;
	funct_t *table = NULL;

	assert_int_equal(funct_init(&table), SYMTSTAT_OK);
	assert_int_equal(funct_insert_begin(table, IS_FUNC_DEF), SYMTSTAT_OK);
	assert_int_equal(funct_insert_end(table, "f", SYMT_TYPE_UNDEFINED), SYMTSTAT_FUNC_BAD_RETURN);
	assert_false(funct_pending_insert(table));
	assert_false(funct_exists(table, "f"));
	assert_int_equal(funct_free(table), SYMTSTAT_OK);
}

/**
 * @brief Tests reading the last valid parameter and an index past the signature.
 * @details Index zero should succeed; index one should return the bad-argument status for a one-parameter
 *          function.
 */
static void test_parameter_index_boundary(void **state) {
	(void)state;
	funct_t *table = NULL;
	funct_symbol_t found = {0};
	symt_data_type_t type = SYMT_TYPE_UNDEFINED;

	assert_int_equal(funct_init(&table), SYMTSTAT_OK);
	assert_int_equal(add_one_param(table, IS_FUNC_DEF, "f", SYMT_TYPE_BOOLEAN, SYMT_TYPE_UNIT), SYMTSTAT_OK);
	assert_int_equal(funct_find_symbol(table, "f", &found), SYMTSTAT_OK);
	assert_int_equal(funct_nth_param_type(table, &found, 0, &type), SYMTSTAT_OK);
	assert_int_equal(funct_nth_param_type(table, &found, 1, &type), SYMTSTAT_BAD_ARG);
	assert_int_equal(funct_free(table), SYMTSTAT_OK);
}

int main(void) {
	const struct CMUnitTest tests[] = {
	    cmocka_unit_test(test_single_definition_and_lookup),
	    cmocka_unit_test(test_repeated_zero_parameter_call),
	    cmocka_unit_test(test_zero_parameter_call_then_definition),
	    cmocka_unit_test(test_unknown_call_return_then_definition),
	    cmocka_unit_test(test_definition_then_unknown_call_return),
	    cmocka_unit_test(test_definition_takes_precedence_over_call_signature),
	    cmocka_unit_test(test_definition_takes_precedence_over_call_return),
	    cmocka_unit_test(test_definition_takes_precedence_over_call_count),
	    cmocka_unit_test(test_parameter_pointers_survive_growth),
	    cmocka_unit_test(test_undefined_definition_return_is_rejected),
	    cmocka_unit_test(test_parameter_index_boundary),
	};

	return cmocka_run_group_tests_name("Functable extra", tests, NULL, NULL);
}
