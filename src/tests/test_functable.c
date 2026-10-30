/**
   @file test_functable.c
   @author xbosakd00 (DasBean125)
   @date 2025-10-28

   @brief Tests the functable module in its entirety.

   Now featuring 50% more headaches.
*/

#define UNIT_UNDER_TEST

#include "../include/functable.h"
#include "../include/functable_impl.h"
#include "../include/hashtable_impl.h"

#include "cmocka/cmocka.h"

#include <stddef.h>
#include <stdio.h>

#define EXEC_TEST_CATEGORY(name, tests)                                                                      \
	do {                                                                                                     \
		printf("\n[    " name "    ]\n");                                                                    \
		test_results |= (cmocka_run_group_tests_name((name), (tests), NULL, NULL));                          \
	} while (0)

#define FREE_TABLE_NOT_FUNCTION(tbl)                                                                         \
	do {                                                                                                     \
		free((tbl)->call_type_array);                                                                        \
		free((tbl)->def_type_array);                                                                         \
		HTBL_FREE((tbl)->scope);                                                                             \
		free((tbl));                                                                                         \
	} while (0)

#define ASSERT_PENDING_SYMBOL_EMPTY(tbl)                                                                     \
	do {                                                                                                     \
		assert_null((tbl)->pending_symbol.def_param_type_array);                                             \
		assert_null((tbl)->pending_symbol.call_param_type_array);                                            \
		assert_int_equal((tbl)->pending_symbol.param_count, -1);                                             \
		assert_int_equal((tbl)->pending_symbol.ret_type, SYMT_TYPE_UNDEFINED);                               \
	} while (0)

/*================
  funct_init tests
  ================*/
static void funct_init_returns(void **state) {
	(void)state;
	assert_int_equal(funct_init(NULL), SYMTSTAT_BAD_ARG);
}

static void funct_init_single(void **state) {
	(void)state;

	funct_t *new_table;
	assert_int_equal(funct_init(&new_table), SYMTSTAT_OK);
	assert_non_null(new_table->call_type_array);
	assert_non_null(new_table->def_type_array);

	assert_uint_equal(new_table->call_arr_count, 0);
	assert_uint_equal(new_table->call_arr_im_count, 0);
	assert_uint_equal(new_table->call_arr_size, 1024); /* make sure this matches FUNCT_INIT_TYPE_ARRAY_SIZE */

	assert_uint_equal(new_table->def_arr_count, 0);
	assert_uint_equal(new_table->def_arr_im_count, 0);
	assert_uint_equal(new_table->def_arr_size, 1024); /* likewise here */

	assert_false(new_table->state_insert);
	ASSERT_PENDING_SYMBOL_EMPTY(new_table);

	FREE_TABLE_NOT_FUNCTION(new_table);
}

/*================
  funct_free tests
  ================*/
static void funct_free_returns(void **state) {
	(void)state;
	assert_int_equal(funct_free(NULL), SYMTSTAT_BAD_ARG);
}

static void funct_free_single(void **state) {
	(void)state;
	funct_t *table;
	funct_init(&table);

	assert_int_equal(funct_free(table), SYMTSTAT_OK);
}

/*========================
  funct_insert_begin tests
  ========================*/
static void funct_insert_begin_returns(void **state) {
	(void)state;
	funct_t *table;
	funct_init(&table);

	assert_int_equal(funct_insert_begin(NULL), SYMTSTAT_BAD_ARG);

	funct_free(table);
}

static void funct_insert_begin_state_transition(void **state) {
	(void)state;
	funct_t *table;
	funct_init(&table);

	assert_int_equal(funct_insert_begin(table), SYMTSTAT_OK);
	assert_true(table->state_insert);
	ASSERT_PENDING_SYMBOL_EMPTY(table);

	assert_int_equal(funct_insert_begin(table), SYMTSTAT_BAD_STATE);

	funct_free(table);
}

/*============================
  funct_insert_add_param tests
  ============================*/
static void funct_insert_add_param_returns(void **state) {
	(void)state;
	funct_t *table;
	funct_init(&table);

	/* Not inserting */
	assert_int_equal(funct_insert_add_param(table, SYMT_TYPE_BOOLEAN, IS_FUNC_CALL), SYMTSTAT_BAD_STATE);
	funct_insert_begin(table);

	/* Inserting but with wrong arguments */
	const symt_data_type_t valid_param_types[] = {SYMT_TYPE_BOOLEAN, SYMT_TYPE_DOUBLE, SYMT_TYPE_STRING,
	                                              SYMT_TYPE_INTEGER, SYMT_TYPE_UNIT};
	/* Overkill */
	for (unsigned i = 0; i < (sizeof(valid_param_types) / sizeof(symt_data_type_t)); i++) {
		assert_int_equal(funct_insert_add_param(NULL, valid_param_types[i], IS_FUNC_DEF), SYMTSTAT_BAD_ARG);
		assert_int_equal(funct_insert_add_param(NULL, valid_param_types[i], IS_FUNC_CALL), SYMTSTAT_BAD_ARG);
	}

	assert_int_equal(funct_insert_add_param(table, SYMT_TYPE_UNDEFINED, IS_FUNC_DEF), SYMTSTAT_BAD_ARG);
	assert_int_equal(table->pending_symbol.param_count, -1);
	assert_int_equal(funct_insert_add_param(table, SYMT_TYPE_UNDEFINED, IS_FUNC_CALL), SYMTSTAT_BAD_ARG);
	assert_int_equal(table->pending_symbol.param_count, -1);

	funct_free(table);
}

static void funct_insert_add_param_call_single(void **state) {
	(void)state;
	funct_t *table;
	funct_init(&table);
	funct_insert_begin(table);

	/* First parameter */
	assert_int_equal(funct_insert_add_param(table, SYMT_TYPE_BOOLEAN, IS_FUNC_CALL), SYMTSTAT_OK);
	assert_null(table->pending_symbol.def_param_type_array);
	assert_non_null(table->pending_symbol.call_param_type_array);
	assert_uint_equal(table->call_arr_im_count, 1);

	assert_int_equal(table->pending_symbol.param_count, 1);
	assert_int_equal(table->pending_symbol.ret_type, SYMT_TYPE_UNDEFINED);
	assert_int_equal(
	    (symt_data_type_t)table->pending_symbol.call_param_type_array[table->pending_symbol.param_count - 1],
	    SYMT_TYPE_BOOLEAN);

	funct_free(table);
}

static void funct_insert_add_param_call_multi(void **state) {
	(void)state;
	funct_t *table;
	funct_init(&table);
	funct_insert_begin(table);

	/* 5 valid parameters */
	const symt_data_type_t param_types[] = {SYMT_TYPE_BOOLEAN, SYMT_TYPE_STRING, SYMT_TYPE_UNIT,
	                                        SYMT_TYPE_DOUBLE, SYMT_TYPE_INTEGER};
	const unsigned param_count = (sizeof(param_types) / sizeof(symt_data_type_t));
	for (unsigned i = 0; i < param_count; i++) {
		assert_int_equal(funct_insert_add_param(table, param_types[i], IS_FUNC_CALL), SYMTSTAT_OK);
		assert_uint_equal(table->call_arr_im_count, i + 1);
	}

	assert_null(table->pending_symbol.def_param_type_array);
	assert_non_null(table->pending_symbol.call_param_type_array);
	assert_int_equal(table->pending_symbol.param_count, (int)param_count);

	for (unsigned i = 0; i < param_count; i++)
		assert_int_equal(table->pending_symbol.call_param_type_array[i], param_types[i]);

	/* Attempt to add invalid param (shouldn't change anything) */
	assert_int_equal(funct_insert_add_param(table, SYMT_TYPE_UNDEFINED, IS_FUNC_CALL), SYMTSTAT_BAD_ARG);
	assert_int_equal(table->pending_symbol.param_count, (int)param_count);
	for (unsigned i = 0; i < param_count; i++)
		assert_int_equal(table->pending_symbol.call_param_type_array[i], param_types[i]);

	funct_free(table);
}

static void funct_insert_add_param_def_single(void **state) {
	(void)state;
	funct_t *table;
	funct_init(&table);
	funct_insert_begin(table);

	/* First parameter */
	assert_int_equal(funct_insert_add_param(table, SYMT_TYPE_BOOLEAN, IS_FUNC_DEF), SYMTSTAT_OK);
	assert_null(table->pending_symbol.call_param_type_array); /* Mustn't change */
	assert_non_null(table->pending_symbol.def_param_type_array);
	assert_uint_equal(table->def_arr_im_count, 1);

	assert_int_equal(table->pending_symbol.param_count, 1);
	assert_int_equal(table->pending_symbol.ret_type, SYMT_TYPE_UNDEFINED); /* Mustn't change */
	assert_int_equal(table->pending_symbol.def_param_type_array[table->pending_symbol.param_count - 1],
	                 SYMT_TYPE_BOOLEAN);

	funct_free(table);
}

static void funct_insert_add_param_def_multi(void **state) {
	(void)state;
	funct_t *table;
	funct_init(&table);
	funct_insert_begin(table);

	/* 5 valid parameters */
	const symt_data_type_t param_types[] = {SYMT_TYPE_BOOLEAN, SYMT_TYPE_STRING, SYMT_TYPE_UNIT,
	                                        SYMT_TYPE_DOUBLE, SYMT_TYPE_INTEGER};
	const unsigned param_count = (sizeof(param_types) / sizeof(symt_data_type_t));
	for (unsigned i = 0; i < param_count; i++) {
		assert_int_equal(funct_insert_add_param(table, param_types[i], IS_FUNC_DEF), SYMTSTAT_OK);
		assert_uint_equal(table->def_arr_im_count, i + 1);
	}

	assert_null(table->pending_symbol.call_param_type_array);
	assert_non_null(table->pending_symbol.def_param_type_array);
	assert_int_equal(table->pending_symbol.param_count, (int)param_count);

	/* Test if param types match what we added */
	for (unsigned i = 0; i < param_count; i++)
		assert_int_equal(table->pending_symbol.def_param_type_array[i], param_types[i]);

	/* Attempt to add invalid param (shouldn't change anything) */
	assert_int_equal(funct_insert_add_param(table, SYMT_TYPE_UNDEFINED, IS_FUNC_DEF), SYMTSTAT_BAD_ARG);
	assert_int_equal(table->pending_symbol.param_count, (int)param_count); /* Param count didn't change */
	for (unsigned i = 0; i < param_count; i++)
		assert_int_equal(table->pending_symbol.def_param_type_array[i], param_types[i]);

	funct_free(table);
}

/*======================
  funct_insert_end tests
  ======================*/
static void funct_insert_end_returns(void **state) {
	(void)state;
	funct_t *table;
	funct_init(&table);

	/* Invalid params */
	assert_int_equal(funct_insert_end(NULL, "all alone inside my head", SYMT_TYPE_BOOLEAN, IS_FUNC_CALL),
	                 SYMTSTAT_BAD_ARG);
	assert_int_equal(funct_insert_end(table, NULL, SYMT_TYPE_DOUBLE, IS_FUNC_CALL), SYMTSTAT_BAD_ARG);

	/* Valid arguments but not in insertion state */
	assert_int_equal(
	    funct_insert_end(table, "all alone with these nightmares in my head", SYMT_TYPE_DOUBLE, IS_FUNC_CALL),
	    SYMTSTAT_BAD_STATE);

	/* Valid insertion state, but invalid params */
	funct_insert_begin(table);
	assert_int_equal(funct_insert_end(NULL, "do you get it?", SYMT_TYPE_UNIT, IS_FUNC_DEF), SYMTSTAT_BAD_ARG);
	assert_int_equal(table->def_arr_count, 0);
	assert_int_equal(table->def_arr_im_count, 0);

	assert_int_equal(funct_insert_end(table, NULL, SYMT_TYPE_DOUBLE, IS_FUNC_CALL), SYMTSTAT_BAD_ARG);
	assert_int_equal(table->call_arr_count, 0);
	assert_int_equal(table->call_arr_im_count, 0);

	funct_free(table);
}

static void funct_insert_end_undefined_symbol(void **state) {
	(void)state;
	funct_t *table_call, *table_def;
	funct_init(&table_call);
	funct_init(&table_def);

	/*
	  Adding an undefined symbol is basically equivalent to adding the following IFJ26 function:
	    + definition: def 33MHz(): UNDEF (not a valid IFJ26 type); no params (def_array == NULL,
	  param_count == 0),
	    + call: 33MHz(); return type may be SYMT_TYPE_UNDEFINED, because it's a call.
	  We have to check both of these cases in separate tables, because technically, adding two symbols
	  is an unknown operation right now (not tested yet).
	*/
	/* Adding a call */
	funct_insert_begin(table_call);
	assert_int_equal(funct_insert_end(table_call, "33MHz", SYMT_TYPE_UNDEFINED, IS_FUNC_CALL), SYMTSTAT_OK);
	assert_true(HTBL_EXISTS(table_call->scope, "33MHz"));

	/* Make sure the function cleans up at least the state (the param array indeces will be checked in a
	   different test) */
	assert_false(table_call->state_insert);

	funct_symbol_t func_symbol = {};
	HTBL_RETRIEVE(table_call->scope, "33MHz", func_symbol);
	assert_null(func_symbol.call_param_type_array);
	assert_null(func_symbol.def_param_type_array);
	assert_int_equal(func_symbol.param_count, 0);
	assert_int_equal(func_symbol.ret_type, SYMT_TYPE_UNDEFINED);

	/* Adding an invalid definition (undefined return type) */
	funct_insert_begin(table_def);
	assert_int_equal(funct_insert_end(table_def, "Invalid", SYMT_TYPE_UNDEFINED, IS_FUNC_DEF),
	                 SYMTSTAT_FUNC_BAD_RETURN);
	assert_false(HTBL_EXISTS(table_call->scope, "Invalid"));
	assert_false(table_call->state_insert);

	funct_free(table_call);
	funct_free(table_def);
}

static void funct_insert_end_single_function_call(void **state) {
	(void)state;
	funct_t *table;
	funct_init(&table);

	/* function("Hi"); */
	funct_insert_begin(table);
	funct_insert_add_param(table, SYMT_TYPE_STRING, IS_FUNC_CALL);
	assert_int_equal(funct_insert_end(table, "function", SYMT_TYPE_UNIT, IS_FUNC_CALL), SYMTSTAT_OK);
	assert_true(HTBL_EXISTS(table->scope, "function"));

	/* table check */
	assert_uint_equal(table->call_arr_count, table->call_arr_im_count);
	assert_uint_equal(table->call_arr_count, 1);   /* index has advanced in the call array */
	assert_uint_equal(table->call_arr_size, 1024); /* no resizing happened */

	/* symbol check */
	funct_symbol_t symbol = {};
	HTBL_RETRIEVE(table->scope, "function", symbol);
	assert_null(symbol.def_param_type_array);
	assert_non_null(symbol.call_param_type_array);
	assert_int_equal(symbol.param_count, 1);
	assert_int_equal(symbol.ret_type, SYMT_TYPE_UNIT);
	assert_int_equal(symbol.call_param_type_array[symbol.param_count - 1], SYMT_TYPE_STRING);

	funct_free(table);
}

static void funct_insert_end_single_function_def(void **state) {
	(void)state;
	funct_t *table;
	funct_init(&table);

	/* function("Hi"); */
	funct_insert_begin(table);
	funct_insert_add_param(table, SYMT_TYPE_DOUBLE, IS_FUNC_DEF);
	assert_int_equal(funct_insert_end(table, "function", SYMT_TYPE_UNIT, IS_FUNC_DEF), SYMTSTAT_OK);
	assert_true(HTBL_EXISTS(table->scope, "function"));

	/* table check */
	assert_uint_equal(table->def_arr_count, table->def_arr_im_count);
	assert_uint_equal(table->def_arr_count, 1);   /* index has advanced in the def array */
	assert_uint_equal(table->def_arr_size, 1024); /* no resizing happened */

	/* symbol check */
	funct_symbol_t symbol = {};
	HTBL_RETRIEVE(table->scope, "function", symbol);
	assert_null(symbol.call_param_type_array);
	assert_non_null(symbol.def_param_type_array);
	assert_int_equal(symbol.param_count, 1);
	assert_int_equal(symbol.ret_type, SYMT_TYPE_UNIT);
	assert_int_equal(symbol.def_param_type_array[symbol.param_count - 1], SYMT_TYPE_DOUBLE);

	funct_free(table);
}

/*==========================
  funct_pending_insert tests
  ==========================*/
static void funct_pending_insert_returns(void **state) {
	(void)state;
	funct_t *table;

	assert_false(funct_pending_insert(NULL));
	funct_init(&table);
	assert_false(funct_pending_insert(table)); /* empty table, should be false by default */

	funct_free(table);
}

static void funct_pending_insert_test(void **state) {
	(void)state;
	funct_t *table;
	funct_init(&table);

	funct_insert_begin(table);
	assert_true(funct_pending_insert(table));

	funct_free(table);
}

static void funct_pending_insert_with_insert_end_fail(void **state) {
	(void)state;
	funct_t *table;
	funct_init(&table);

	funct_insert_begin(table);
	funct_insert_end(table, "wrong", SYMT_TYPE_UNDEFINED, IS_FUNC_DEF);
	assert_false(funct_pending_insert(table));

	funct_free(table);
}

static void funct_pending_insert_with_insert_end_success(void **state) {
	(void)state;
	funct_t *table;
	funct_init(&table);

	funct_insert_begin(table);
	funct_insert_end(table, "right", SYMT_TYPE_UNIT, IS_FUNC_DEF);
	assert_false(funct_pending_insert(table));

	funct_free(table);
}

/*==================
  funct_exists tests
  ==================*/
static void funct_exists_returns(void **state) {
	(void)state;
	funct_t *table;
	funct_init(&table);

	assert_false(funct_exists(NULL, "why can't we"));
	assert_false(funct_exists(table, NULL));
	assert_false(funct_exists(table, "just admit it?")); /* empty table */

	funct_free(table);
}

static void funct_exists_single(void **state) {
	(void)state;
	funct_t *table;
	funct_init(&table);

	funct_insert_begin(table);
	funct_insert_end(table, "CPU", SYMT_TYPE_BOOLEAN, IS_FUNC_DEF);
	assert_true(funct_exists(table, "CPU"));

	funct_free(table);
}

/*=============================
  Advanced funct_insert_* tests
  =============================*/
// TODO: add a bunch of tests that test adding multiple symbols, as well as symbol conflicts and all that shit

/* The funny */
int main(void) {
	struct CMUnitTest tests_funct_init[] = {
	    cmocka_unit_test(funct_init_returns),
	    cmocka_unit_test(funct_init_single),
	};
	struct CMUnitTest tests_funct_free[] = {
	    cmocka_unit_test(funct_free_returns),
	    cmocka_unit_test(funct_free_single),
	};

	struct CMUnitTest tests_funct_insert_begin[] = {
	    cmocka_unit_test(funct_insert_begin_returns),
	    cmocka_unit_test(funct_insert_begin_state_transition),
	};
	struct CMUnitTest tests_funct_insert_add_param[] = {
	    cmocka_unit_test(funct_insert_add_param_returns),
	    cmocka_unit_test(funct_insert_add_param_call_single),
	    cmocka_unit_test(funct_insert_add_param_call_multi),
	    cmocka_unit_test(funct_insert_add_param_def_single),
	    cmocka_unit_test(funct_insert_add_param_def_multi),
	};
	struct CMUnitTest tests_funct_insert_end[] = {
	    cmocka_unit_test(funct_insert_end_returns),
	    cmocka_unit_test(funct_insert_end_undefined_symbol),
	    cmocka_unit_test(funct_insert_end_single_function_call),
	    cmocka_unit_test(funct_insert_end_single_function_def),
	};

	struct CMUnitTest tests_funct_pending_insert[] = {
	    cmocka_unit_test(funct_pending_insert_returns),
	    cmocka_unit_test(funct_pending_insert_test),
	    cmocka_unit_test(funct_pending_insert_with_insert_end_fail),
	    cmocka_unit_test(funct_pending_insert_with_insert_end_success),
	};
	struct CMUnitTest tests_funct_exists[] = {
	    cmocka_unit_test(funct_exists_returns),
	    cmocka_unit_test(funct_exists_single),
	};

	int test_results = 0;

	EXEC_TEST_CATEGORY("funct_init", tests_funct_init);
	EXEC_TEST_CATEGORY("funct_free", tests_funct_free);

	EXEC_TEST_CATEGORY("funct_insert_begin", tests_funct_insert_begin);
	EXEC_TEST_CATEGORY("funct_insert_add_param", tests_funct_insert_add_param);
	EXEC_TEST_CATEGORY("funct_insert_end", tests_funct_insert_end);

	EXEC_TEST_CATEGORY("funct_pending_insert", tests_funct_pending_insert);
	EXEC_TEST_CATEGORY("funct_exists", tests_funct_exists);

	return test_results;
}
