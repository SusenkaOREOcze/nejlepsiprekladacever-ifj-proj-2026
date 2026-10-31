/**
   @file test_functable.c
   @author xbosakd00 (DasBean125)
   @date 2026-10-28

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

/*=============================
  funct_resize_type_array tests
  =============================*/
static void funct_resize_type_array_returns(void **state) {
	(void)state;
	assert_int_equal(funct_resize_type_array(NULL, 42), 1);
}

static void funct_resize_type_array_null_allocate(void **state) {
	(void)state;
	int8_t *array = NULL;
	assert_int_equal(funct_resize_type_array(&array, 42), 0);

	assert_non_null(array);

	free(array);
}

/*================================
  funct_append_def/call_type tests
  ================================*/
static void funct_append_def_type_returns(void **state) {
	(void)state;
	assert_int_equal(funct_append_def_type(NULL, SYMT_TYPE_BOOLEAN), 1);
}

static void funct_append_def_type_single(void **state) {
	(void)state;
	funct_t *table;
	funct_init(&table);

	assert_int_equal(funct_append_def_type(table, SYMT_TYPE_STRING), 0);
	assert_uint_equal(table->def_arr_im_count, 1);
	assert_int_equal(table->def_type_array[0], SYMT_TYPE_STRING);

	funct_free(table);
}

static void funct_append_def_type_resize(void **state) {
	(void)state;
	funct_t *table;
	funct_init(&table);

	const size_t prev_def_size = table->def_arr_size;
	for (int i = 0; i < 1025; i++) {
		assert_int_equal(funct_append_def_type(table, SYMT_TYPE_DOUBLE), 0);
		assert_uint_equal(table->def_arr_im_count, (size_t)(i + 1));
		assert_int_equal(table->def_type_array[i], SYMT_TYPE_DOUBLE);
	}
	assert_uint_equal(table->def_arr_size, 2 * prev_def_size);

	funct_free(table);
}

static void funct_append_call_type_returns(void **state) {
	(void)state;
	assert_int_equal(funct_append_call_type(NULL, SYMT_TYPE_BOOLEAN), 1);
}

static void funct_append_call_type_single(void **state) {
	(void)state;
	funct_t *table;
	funct_init(&table);

	assert_int_equal(funct_append_call_type(table, SYMT_TYPE_STRING), 0);
	assert_uint_equal(table->call_arr_im_count, 1);
	assert_int_equal(table->call_type_array[0], SYMT_TYPE_STRING);

	funct_free(table);
}

static void funct_append_call_type_resize(void **state) {
	(void)state;
	funct_t *table;
	funct_init(&table);

	const size_t prev_call_size = table->call_arr_size;
	for (int i = 0; i < 1025; i++) {
		assert_int_equal(funct_append_call_type(table, SYMT_TYPE_INTEGER), 0);
		assert_uint_equal(table->call_arr_im_count, (size_t)(i + 1));
		assert_int_equal(table->call_type_array[i], SYMT_TYPE_INTEGER);
	}
	assert_uint_equal(table->call_arr_size, 2 * prev_call_size);

	funct_free(table);
}

/*========================
  funct_insert_begin tests
  ========================*/
static void funct_insert_begin_returns(void **state) {
	(void)state;
	funct_t *table;
	funct_init(&table);

	assert_int_equal(funct_insert_begin(NULL, IS_FUNC_CALL), SYMTSTAT_BAD_ARG);

	funct_free(table);
}

static void funct_insert_begin_state_transition(void **state) {
	(void)state;
	funct_t *table;
	funct_init(&table);

	assert_int_equal(funct_insert_begin(table, IS_FUNC_CALL), SYMTSTAT_OK);
	assert_true(table->state_insert);
	ASSERT_PENDING_SYMBOL_EMPTY(table);

	assert_int_equal(funct_insert_begin(table, IS_FUNC_CALL), SYMTSTAT_BAD_STATE);

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
	assert_int_equal(funct_insert_add_param(table, SYMT_TYPE_BOOLEAN), SYMTSTAT_BAD_STATE);
	funct_insert_begin(table, IS_FUNC_CALL);

	/* Inserting but with wrong arguments */
	const symt_data_type_t valid_param_types[] = {SYMT_TYPE_BOOLEAN, SYMT_TYPE_DOUBLE, SYMT_TYPE_STRING,
	                                              SYMT_TYPE_INTEGER, SYMT_TYPE_UNIT};
	/* Overkill */
	for (unsigned i = 0; i < (sizeof(valid_param_types) / sizeof(symt_data_type_t)); i++) {
		assert_int_equal(funct_insert_add_param(NULL, valid_param_types[i]), SYMTSTAT_BAD_ARG);
	}

	assert_int_equal(funct_insert_add_param(table, SYMT_TYPE_UNDEFINED), SYMTSTAT_BAD_ARG);
	assert_int_equal(table->pending_symbol.param_count, -1);
	assert_int_equal(funct_insert_add_param(table, SYMT_TYPE_UNDEFINED), SYMTSTAT_BAD_ARG);
	assert_int_equal(table->pending_symbol.param_count, -1);

	funct_free(table);
}

static void funct_insert_add_param_call_single(void **state) {
	(void)state;
	funct_t *table;
	funct_init(&table);
	funct_insert_begin(table, IS_FUNC_CALL);

	/* First parameter */
	assert_int_equal(funct_insert_add_param(table, SYMT_TYPE_BOOLEAN), SYMTSTAT_OK);
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
	funct_insert_begin(table, IS_FUNC_CALL);

	/* 5 valid parameters */
	const symt_data_type_t param_types[] = {SYMT_TYPE_BOOLEAN, SYMT_TYPE_STRING, SYMT_TYPE_UNIT,
	                                        SYMT_TYPE_DOUBLE, SYMT_TYPE_INTEGER};
	const unsigned param_count = (sizeof(param_types) / sizeof(symt_data_type_t));
	for (unsigned i = 0; i < param_count; i++) {
		assert_int_equal(funct_insert_add_param(table, param_types[i]), SYMTSTAT_OK);
		assert_uint_equal(table->call_arr_im_count, i + 1);
	}

	assert_null(table->pending_symbol.def_param_type_array);
	assert_non_null(table->pending_symbol.call_param_type_array);
	assert_int_equal(table->pending_symbol.param_count, (int)param_count);

	for (unsigned i = 0; i < param_count; i++)
		assert_int_equal(table->pending_symbol.call_param_type_array[i], param_types[i]);

	/* Attempt to add invalid param (shouldn't change anything) */
	assert_int_equal(funct_insert_add_param(table, SYMT_TYPE_UNDEFINED), SYMTSTAT_BAD_ARG);
	assert_int_equal(table->pending_symbol.param_count, (int)param_count);
	for (unsigned i = 0; i < param_count; i++)
		assert_int_equal(table->pending_symbol.call_param_type_array[i], param_types[i]);

	funct_free(table);
}

static void funct_insert_add_param_def_single(void **state) {
	(void)state;
	funct_t *table;
	funct_init(&table);
	funct_insert_begin(table, IS_FUNC_DEF);

	/* First parameter */
	assert_int_equal(funct_insert_add_param(table, SYMT_TYPE_BOOLEAN), SYMTSTAT_OK);
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
	funct_insert_begin(table, IS_FUNC_DEF);

	/* 5 valid parameters */
	const symt_data_type_t param_types[] = {SYMT_TYPE_BOOLEAN, SYMT_TYPE_STRING, SYMT_TYPE_UNIT,
	                                        SYMT_TYPE_DOUBLE, SYMT_TYPE_INTEGER};
	const unsigned param_count = (sizeof(param_types) / sizeof(symt_data_type_t));
	for (unsigned i = 0; i < param_count; i++) {
		assert_int_equal(funct_insert_add_param(table, param_types[i]), SYMTSTAT_OK);
		assert_uint_equal(table->def_arr_im_count, i + 1);
	}

	assert_null(table->pending_symbol.call_param_type_array);
	assert_non_null(table->pending_symbol.def_param_type_array);
	assert_int_equal(table->pending_symbol.param_count, (int)param_count);

	/* Test if param types match what we added */
	for (unsigned i = 0; i < param_count; i++)
		assert_int_equal(table->pending_symbol.def_param_type_array[i], param_types[i]);

	/* Attempt to add invalid param (shouldn't change anything) */
	assert_int_equal(funct_insert_add_param(table, SYMT_TYPE_UNDEFINED), SYMTSTAT_BAD_ARG);
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

	/* Invalid arguments */
	assert_int_equal(funct_insert_end(NULL, "all alone inside my head", SYMT_TYPE_BOOLEAN), SYMTSTAT_BAD_ARG);
	assert_int_equal(funct_insert_end(table, NULL, SYMT_TYPE_DOUBLE), SYMTSTAT_BAD_ARG);

	/* Valid arguments but not in insertion state */
	assert_int_equal(funct_insert_end(table, "all alone with these nightmares in my head", SYMT_TYPE_DOUBLE),
	                 SYMTSTAT_BAD_STATE);

	/* Valid insertion state, but invalid params */
	funct_insert_begin(table, IS_FUNC_DEF);
	assert_int_equal(funct_insert_end(NULL, "do you get it?", SYMT_TYPE_UNIT), SYMTSTAT_BAD_ARG);
	assert_int_equal(table->def_arr_count, 0);
	assert_int_equal(table->def_arr_im_count, 0);
	assert_true(table->state_insert);

	assert_int_equal(funct_insert_end(table, NULL, SYMT_TYPE_DOUBLE), SYMTSTAT_BAD_ARG);
	assert_int_equal(table->def_arr_count, 0);
	assert_int_equal(table->def_arr_im_count, 0);
	assert_true(table->state_insert);

	assert_int_equal(funct_insert_end(table, "", SYMT_TYPE_INTEGER), SYMTSTAT_BAD_ARG);
	assert_int_equal(table->def_arr_count, 0);
	assert_int_equal(table->def_arr_im_count, 0);
	assert_true(table->state_insert);

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
	funct_insert_begin(table_call, IS_FUNC_CALL);
	assert_int_equal(funct_insert_end(table_call, "33MHz", SYMT_TYPE_UNDEFINED), SYMTSTAT_OK);
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
	funct_insert_begin(table_def, IS_FUNC_DEF);
	assert_int_equal(funct_insert_end(table_def, "Invalid", SYMT_TYPE_UNDEFINED), SYMTSTAT_FUNC_BAD_RETURN);
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
	funct_insert_begin(table, IS_FUNC_CALL);
	funct_insert_add_param(table, SYMT_TYPE_STRING);
	assert_int_equal(funct_insert_end(table, "function", SYMT_TYPE_UNIT), SYMTSTAT_OK);
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

	/* def function(f: Double): Unit; */
	funct_insert_begin(table, IS_FUNC_DEF);
	funct_insert_add_param(table, SYMT_TYPE_DOUBLE);
	assert_int_equal(funct_insert_end(table, "function", SYMT_TYPE_UNIT), SYMTSTAT_OK);
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

/*=======================
  funct_find_symbol tests
  =======================*/
static void funct_find_symbol_returns(void **state) {
	(void)state;
	funct_t *table;
	funct_init(&table);
	funct_symbol_t sym_out = {
	    .call_param_type_array = NULL,
	    .def_param_type_array = NULL,
	    .param_count = -1,
	    .ret_type = SYMT_TYPE_UNDEFINED,
	};

	assert_int_equal(funct_find_symbol(NULL, "hello", &sym_out), SYMTSTAT_BAD_ARG);
	assert_int_equal(funct_find_symbol(table, NULL, &sym_out), SYMTSTAT_BAD_ARG);
	assert_int_equal(funct_find_symbol(table, "hello", NULL), SYMTSTAT_BAD_ARG);

	funct_free(table);
}

static void funct_find_symbol_empty_table(void **state) {
	(void)state;
	funct_t *table;
	funct_init(&table);
	funct_symbol_t sym_out = {
	    .call_param_type_array = NULL,
	    .def_param_type_array = NULL,
	    .param_count = -1,
	    .ret_type = SYMT_TYPE_UNDEFINED,
	};

	assert_int_equal(funct_find_symbol(table, "nonexistent", &sym_out), SYMTSTAT_NO_SYMBOL);
	assert_int_equal(sym_out.param_count, -1);
	assert_int_equal(funct_find_symbol(table, "", &sym_out), SYMTSTAT_NO_SYMBOL);
	assert_int_equal(sym_out.param_count, -1);

	funct_free(table);
}

/*==========================
  funct_nth_param_type tests
  ==========================*/
static void funct_nth_param_type_returns(void **state) {
	(void)state;

	int8_t call_types[] = {(int8_t)SYMT_TYPE_INTEGER};
	funct_symbol_t *sym_in = malloc(sizeof(funct_symbol_t));
	sym_in->param_count = 1;
	sym_in->call_param_type_array = call_types;
	sym_in->ret_type = SYMT_TYPE_UNIT;

	symt_data_type_t type_out = SYMT_TYPE_UNDEFINED;

	assert_int_equal(funct_nth_param_type(NULL, 1, &type_out), SYMTSTAT_BAD_ARG);
	assert_int_equal(type_out, SYMT_TYPE_UNDEFINED);
	assert_int_equal(funct_nth_param_type(sym_in, -32809732, &type_out), SYMTSTAT_BAD_ARG);
	assert_int_equal(type_out, SYMT_TYPE_UNDEFINED);
	assert_int_equal(funct_nth_param_type(sym_in, 0, NULL), SYMTSTAT_BAD_ARG);
	assert_int_equal(type_out, SYMT_TYPE_UNDEFINED);

	/* Invalid index in a function symbol with 1 parameter */
	assert_int_equal(funct_nth_param_type(sym_in, 1, &type_out), SYMTSTAT_BAD_ARG);
	assert_int_equal(type_out, SYMT_TYPE_UNDEFINED);

	free(sym_in);
}

static void funct_nth_param_type_parameterless_symbol(void **state) {
	(void)state;

	funct_symbol_t *sym_in = malloc(sizeof(funct_symbol_t));
	sym_in->param_count = 0;
	sym_in->ret_type = SYMT_TYPE_UNIT;
	symt_data_type_t type_out = SYMT_TYPE_UNDEFINED;

	assert_int_equal(funct_nth_param_type(sym_in, 0, &type_out), SYMTSTAT_FUNC_NO_PARAMS);
	assert_int_equal(type_out, SYMT_TYPE_UNDEFINED);
	assert_int_equal(funct_nth_param_type(sym_in, 2, &type_out), SYMTSTAT_FUNC_NO_PARAMS);
	assert_int_equal(type_out, SYMT_TYPE_UNDEFINED);
	assert_int_equal(funct_nth_param_type(sym_in, -1, &type_out), SYMTSTAT_BAD_ARG);
	assert_int_equal(type_out, SYMT_TYPE_UNDEFINED);

	free(sym_in);
}

static void funct_nth_param_type_def_precedence(void **state) {
	(void)state;

	int8_t call_types[] = {(int8_t)SYMT_TYPE_STRING};
	int8_t def_types[] = {(int8_t)SYMT_TYPE_DOUBLE, (int8_t)SYMT_TYPE_INTEGER};
	funct_symbol_t *sym_in = malloc(sizeof(funct_symbol_t));
	sym_in->param_count = 2;
	sym_in->call_param_type_array = call_types;
	sym_in->def_param_type_array = def_types;
	sym_in->ret_type = SYMT_TYPE_UNIT;

	symt_data_type_t type_out = SYMT_TYPE_UNDEFINED;

	assert_int_equal(funct_nth_param_type(sym_in, 0, &type_out), SYMTSTAT_OK);
	assert_int_equal(type_out, SYMT_TYPE_DOUBLE);
	assert_int_equal(funct_nth_param_type(sym_in, 1, &type_out), SYMTSTAT_OK);
	assert_int_equal(type_out, SYMT_TYPE_INTEGER);
	assert_int_equal(funct_nth_param_type(sym_in, 2, &type_out), SYMTSTAT_BAD_ARG);
	assert_int_equal(type_out, SYMT_TYPE_INTEGER);

	free(sym_in);
}

/*==================================
  funct_compare_sym_signatures tests
  ==================================*/
static void funct_compare_sym_signatures_returns(void **state) {
	(void)state;

	funct_symbol_t sym1 = {
	    .call_param_type_array = NULL,
	    .def_param_type_array = NULL,
	    .param_count = -1,
	    .ret_type = SYMT_TYPE_UNDEFINED,
	};
	funct_symbol_t sym2 = {
	    .call_param_type_array = NULL,
	    .def_param_type_array = NULL,
	    .param_count = -1,
	    .ret_type = SYMT_TYPE_UNDEFINED,
	};

	assert_int_equal(funct_compare_sym_signatures(NULL, &sym2), SYMTSTAT_BAD_ARG);
	assert_int_equal(funct_compare_sym_signatures(&sym1, NULL), SYMTSTAT_BAD_ARG);
	assert_int_equal(funct_compare_sym_signatures(&sym1, &sym2), SYMTSTAT_BAD_ARG);

	sym1.call_param_type_array = (int8_t *)0xDEADBEEF; /* this is insane */
	sym1.def_param_type_array = (int8_t *)0xDEADBEEF;
	assert_int_equal(funct_compare_sym_signatures(&sym1, &sym2), SYMTSTAT_BAD_ARG);
	sym2.call_param_type_array = (int8_t *)0xDEADBEEF; /* really edging the segfault with this one */
	sym2.def_param_type_array = (int8_t *)0xDEADBEEF;

	sym1.param_count = 3; /* if just one of these is -1, the function should fail */
	assert_int_equal(funct_compare_sym_signatures(&sym1, &sym2), SYMTSTAT_BAD_ARG);
	sym1.param_count = -1;
	sym2.param_count = 42;
	assert_int_equal(funct_compare_sym_signatures(&sym1, &sym2), SYMTSTAT_BAD_ARG);
}

static void funct_compare_sym_signatures_by_call_sign(void **state) {
	(void)state;
	int8_t signature1[] = {SYMT_TYPE_BOOLEAN, SYMT_TYPE_BOOLEAN};
	int8_t signature2[] = {SYMT_TYPE_BOOLEAN, SYMT_TYPE_DOUBLE};

	/* Ensure that the base_sym (left side) has only a call signature */
	funct_symbol_t sym1 = {
	    .def_param_type_array = NULL,
	    .call_param_type_array = signature1,
	    .param_count = 2,
	    .ret_type = SYMT_TYPE_UNDEFINED, /* can be anything, this function doesn't care */
	};
	funct_symbol_t sym2 = {
	    .call_param_type_array = signature1,
	    .def_param_type_array = NULL,
	    .param_count = 2,
	    .ret_type = SYMT_TYPE_BOOLEAN,
	};

	assert_int_equal(funct_compare_sym_signatures(&sym1, &sym2), SYMTSTAT_OK);
	sym2.call_param_type_array = signature2;
	assert_int_equal(funct_compare_sym_signatures(&sym1, &sym2), SYMTSTAT_FUNC_DIFF_CALL_SIGN);

	sym2.call_param_type_array = NULL;
	sym2.def_param_type_array = signature1;
	assert_int_equal(funct_compare_sym_signatures(&sym1, &sym2), SYMTSTAT_OK);
	sym2.def_param_type_array = signature2;
	assert_int_equal(funct_compare_sym_signatures(&sym1, &sym2), SYMTSTAT_FUNC_DIFF_CALL_SIGN);
}

static void funct_compare_sym_signatures_by_def_sign(void **state) {
	(void)state;
	int8_t signature1[] = {SYMT_TYPE_BOOLEAN, SYMT_TYPE_BOOLEAN};
	int8_t signature2[] = {SYMT_TYPE_BOOLEAN, SYMT_TYPE_DOUBLE};

	/* Ensure that the base_sym (left side) has only a def signature */
	funct_symbol_t sym1 = {
	    .call_param_type_array = NULL,
	    .def_param_type_array = signature1,
	    .param_count = 2,
	    .ret_type = SYMT_TYPE_STRING,
	};
	funct_symbol_t sym2 = {
	    .call_param_type_array = signature1,
	    .def_param_type_array = NULL,
	    .param_count = 2,
	    .ret_type = SYMT_TYPE_BOOLEAN,
	};

	assert_int_equal(funct_compare_sym_signatures(&sym1, &sym2), SYMTSTAT_OK);
	sym2.call_param_type_array = signature2;
	assert_int_equal(funct_compare_sym_signatures(&sym1, &sym2), SYMTSTAT_FUNC_DIFF_DEF_SIGN);

	sym2.call_param_type_array = NULL;
	sym2.def_param_type_array = signature1;
	assert_int_equal(funct_compare_sym_signatures(&sym1, &sym2), SYMTSTAT_OK);
	sym2.def_param_type_array = signature2;
	assert_int_equal(funct_compare_sym_signatures(&sym1, &sym2), SYMTSTAT_FUNC_DIFF_DEF_SIGN);
}

/*==============================
  funct_copy_update_symbol tests
  ==============================*/
static void funct_copy_update_symbol_returns(void **state) {
	(void)state;

	assert_int_equal(funct_copy_update_symbol(NULL, (funct_symbol_t *)0xDDDD), SYMTSTAT_BAD_ARG);
	assert_int_equal(funct_copy_update_symbol((funct_symbol_t *)0xDDDD, NULL), SYMTSTAT_BAD_ARG);
}

static void funct_copy_update_symbol_field_sequence(void **state) {
	(void)state;
	
    	funct_symbol_t sym1 = {
	    .call_param_type_array = (int8_t *)0xDEADBEEF,
	    .def_param_type_array = (int8_t *)0xDEADCEEF,
	    .param_count = 1,
	    .ret_type = SYMT_TYPE_UNDEFINED,
	};
	funct_symbol_t sym2 = {
	    .call_param_type_array = (int8_t *)0x0BEDBEEF,
	    .def_param_type_array = (int8_t *)0x0BEDCEEF,
	    .param_count = 2,
	    .ret_type = SYMT_TYPE_BOOLEAN,
	};

	assert_int_equal(funct_copy_update_symbol(&sym1, &sym2), SYMTSTAT_OK);
	assert_int_equal(sym1.ret_type, sym2.ret_type);
	assert_int_equal(sym1.param_count, 1);

	sym1.call_param_type_array = NULL;
	assert_int_equal(funct_copy_update_symbol(&sym1, &sym2), SYMTSTAT_OK);
	assert_ptr_equal(sym1.call_param_type_array, sym2.call_param_type_array);
	assert_int_equal(sym1.param_count, 1);

	sym1.def_param_type_array = NULL;
	assert_int_equal(funct_copy_update_symbol(&sym1, &sym2), SYMTSTAT_OK);
	assert_ptr_equal(sym1.def_param_type_array, sym2.def_param_type_array);
	assert_int_equal(sym1.param_count, 1);
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

	funct_insert_begin(table, IS_FUNC_CALL);
	assert_true(funct_pending_insert(table));

	funct_free(table);
}

static void funct_pending_insert_with_insert_end_fail(void **state) {
	(void)state;
	funct_t *table;
	funct_init(&table);

	funct_insert_begin(table, IS_FUNC_DEF);
	funct_insert_end(table, "wrong", SYMT_TYPE_UNDEFINED);
	assert_false(funct_pending_insert(table));

	funct_free(table);
}

static void funct_pending_insert_with_insert_end_success(void **state) {
	(void)state;
	funct_t *table;
	funct_init(&table);

	funct_insert_begin(table, IS_FUNC_DEF);
	funct_insert_end(table, "right", SYMT_TYPE_UNIT);
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

	funct_insert_begin(table, IS_FUNC_DEF);
	funct_insert_end(table, "CPU", SYMT_TYPE_BOOLEAN);
	assert_true(funct_exists(table, "CPU"));

	funct_free(table);
}

/*====================================
  Advanced funct_insert_* tests (CALL)
  ====================================*/
static void funct_insert_begin_state_reset(void **state) {
	(void)state;
	funct_t *table;
	funct_init(&table);

	funct_insert_begin(table, IS_FUNC_CALL);
	assert_int_equal(funct_insert_end(table, "func1", SYMT_TYPE_DOUBLE), SYMTSTAT_OK);

	assert_int_equal(table->pending_symbol.ret_type, SYMT_TYPE_DOUBLE);
	assert_int_equal(table->pending_symbol.param_count, 0);
	assert_int_equal(funct_insert_begin(table, IS_FUNC_CALL), SYMTSTAT_OK); /* pending_symbol state reset */
	assert_int_equal(table->pending_symbol.ret_type, SYMT_TYPE_UNDEFINED);
	assert_int_equal(table->pending_symbol.param_count, -1);

	funct_free(table);
}

static void funct_insert_calls_unique(void **state) {
	(void)state;
	funct_t *table;
	funct_init(&table);

	funct_insert_begin(table, IS_FUNC_CALL);
	funct_insert_add_param(table, SYMT_TYPE_INTEGER);
	assert_int_equal(funct_insert_end(table, "func1", SYMT_TYPE_DOUBLE), SYMTSTAT_OK);
	funct_symbol_t sym_func1;
	assert_int_equal(funct_find_symbol(table, "func1", &sym_func1), SYMTSTAT_OK);

	funct_insert_begin(table, IS_FUNC_CALL);
	funct_insert_add_param(table, SYMT_TYPE_STRING);
	assert_int_equal(funct_insert_end(table, "func2", SYMT_TYPE_UNIT), SYMTSTAT_OK);
	funct_symbol_t sym_func2;
	assert_int_equal(funct_find_symbol(table, "func2", &sym_func2), SYMTSTAT_OK);

	funct_insert_begin(table, IS_FUNC_CALL);
	funct_insert_add_param(table, SYMT_TYPE_UNIT);
	assert_int_equal(funct_insert_end(table, "func3", SYMT_TYPE_UNDEFINED), SYMTSTAT_OK);
	funct_symbol_t sym_func3;
	assert_int_equal(funct_find_symbol(table, "func3", &sym_func3), SYMTSTAT_OK);

	assert_uint_equal(table->call_arr_count, 3);
	assert_uint_equal(table->call_arr_im_count, 3);
	assert_false(table->state_insert);

	assert_int_equal(sym_func1.call_param_type_array[0], SYMT_TYPE_INTEGER);
	assert_int_equal(sym_func1.param_count, 1);
	assert_int_equal(sym_func1.ret_type, SYMT_TYPE_DOUBLE);

	assert_int_equal(sym_func2.call_param_type_array[0], SYMT_TYPE_STRING);
	assert_int_equal(sym_func2.param_count, 1);
	assert_int_equal(sym_func2.ret_type, SYMT_TYPE_UNIT);

	assert_int_equal(sym_func3.call_param_type_array[0], SYMT_TYPE_UNIT);
	assert_int_equal(sym_func3.param_count, 1);
	assert_int_equal(sym_func3.ret_type, SYMT_TYPE_UNDEFINED);

	funct_free(table);
}

static void funct_insert_calls_param_count_conflict(void **state) {
	(void)state;
	funct_t *table;
	funct_init(&table);

	funct_insert_begin(table, IS_FUNC_CALL);
	assert_int_equal(funct_insert_end(table, "func1", SYMT_TYPE_BOOLEAN), SYMTSTAT_OK); /* param_count = 0 */
	funct_symbol_t sym_func1;
	funct_find_symbol(table, "func1", &sym_func1);
	const int prev_param_count = sym_func1.param_count;

	funct_insert_begin(table, IS_FUNC_CALL);
	funct_insert_add_param(table, SYMT_TYPE_STRING); /* param_count = 1 */
	assert_int_equal(funct_insert_end(table, "func1", SYMT_TYPE_BOOLEAN), SYMTSTAT_FUNC_BAD_PARAM_CNT);

	assert_false(table->state_insert);
	funct_find_symbol(table, "func1", &sym_func1);
	assert_int_equal(prev_param_count, sym_func1.param_count);

	funct_free(table);
}

static void funct_insert_calls_signature_conflict(void **state) {
	(void)state;
	funct_t *table;
	funct_init(&table);

	funct_insert_begin(table, IS_FUNC_CALL);
	funct_insert_add_param(table, SYMT_TYPE_DOUBLE); /* 1:DOUBLE */
	assert_int_equal(funct_insert_end(table, "func1", SYMT_TYPE_BOOLEAN), SYMTSTAT_OK);
	funct_symbol_t sym_func1;
	funct_find_symbol(table, "func1", &sym_func1);
	const symt_data_type_t prev_param_type = sym_func1.call_param_type_array[0];

	funct_insert_begin(table, IS_FUNC_CALL);
	funct_insert_add_param(table, SYMT_TYPE_STRING); /* 1:STRING */
	assert_int_equal(funct_insert_end(table, "func1", SYMT_TYPE_BOOLEAN), SYMTSTAT_FUNC_DIFF_CALL_SIGN);

	assert_false(table->state_insert);
	funct_find_symbol(table, "func1", &sym_func1);
	assert_int_equal(prev_param_type, sym_func1.call_param_type_array[0]);

	funct_free(table);
}

static void funct_insert_calls_return_conflict(void **state) {
	(void)state;
	funct_t *table;
	funct_init(&table);

	funct_insert_begin(table, IS_FUNC_CALL);
	funct_insert_add_param(table, SYMT_TYPE_DOUBLE);
	assert_int_equal(funct_insert_end(table, "func1", SYMT_TYPE_DOUBLE), SYMTSTAT_OK); /* ret:DOUBLE */
	funct_symbol_t sym_func1;
	funct_find_symbol(table, "func1", &sym_func1);
	const symt_data_type_t prev_ret_type = sym_func1.ret_type;

	funct_insert_begin(table, IS_FUNC_CALL);
	funct_insert_add_param(table, SYMT_TYPE_DOUBLE);
	assert_int_equal(funct_insert_end(table, "func1", SYMT_TYPE_BOOLEAN),
	                 SYMTSTAT_FUNC_BAD_RETURN); /* ret:BOOLEAN */

	assert_false(table->state_insert);
	funct_find_symbol(table, "func1", &sym_func1);
	assert_int_equal(prev_ret_type, sym_func1.ret_type);

	funct_free(table);
}

static void funct_insert_calls_update_existing(void **state) {
	(void)state;
	funct_t *table;
	funct_init(&table);

	/* By deduction, it's apparent that the only time a call can get updated is when its return type is
	SYMT_TYPE_UNDEFINED */
	funct_insert_begin(table, IS_FUNC_CALL);
	funct_insert_add_param(table, SYMT_TYPE_BOOLEAN);
	assert_int_equal(funct_insert_end(table, "func1", SYMT_TYPE_UNDEFINED), SYMTSTAT_OK);

	funct_symbol_t sym_func1;
	funct_find_symbol(table, "func1", &sym_func1);
	const int8_t *prev_call_adr = sym_func1.call_param_type_array;
	const int8_t *prev_def_adr = sym_func1.def_param_type_array;
	const int prev_param_count = sym_func1.param_count;

	funct_insert_begin(table, IS_FUNC_CALL);
	funct_insert_add_param(table, SYMT_TYPE_BOOLEAN);
	assert_int_equal(funct_insert_end(table, "func1", SYMT_TYPE_STRING), SYMTSTAT_OK);

	assert_false(table->state_insert);
	funct_find_symbol(table, "func1", &sym_func1);
	assert_int_equal(sym_func1.ret_type, SYMT_TYPE_STRING);
	assert_ptr_equal(sym_func1.call_param_type_array, prev_call_adr);
	assert_ptr_equal(sym_func1.def_param_type_array, prev_def_adr);
	assert_int_equal(sym_func1.param_count, prev_param_count);

	funct_free(table);
}

/*===================================
  Advanced funct_insert_* tests (DEF)
  ===================================*/
static void funct_insert_defs_unique(void **state) {
	(void)state;
	funct_t *table;
	funct_init(&table);

	funct_insert_begin(table, IS_FUNC_DEF);
	funct_insert_add_param(table, SYMT_TYPE_STRING);
	assert_int_equal(funct_insert_end(table, "func1", SYMT_TYPE_DOUBLE), SYMTSTAT_OK);
	funct_symbol_t sym_func1;
	assert_int_equal(funct_find_symbol(table, "func1", &sym_func1), SYMTSTAT_OK);

	funct_insert_begin(table, IS_FUNC_DEF);
	funct_insert_add_param(table, SYMT_TYPE_DOUBLE);
	assert_int_equal(funct_insert_end(table, "func2", SYMT_TYPE_UNIT), SYMTSTAT_OK);
	funct_symbol_t sym_func2;
	assert_int_equal(funct_find_symbol(table, "func2", &sym_func2), SYMTSTAT_OK);

	funct_insert_begin(table, IS_FUNC_DEF);
	funct_insert_add_param(table, SYMT_TYPE_INTEGER);
	assert_int_equal(funct_insert_end(table, "func3", SYMT_TYPE_BOOLEAN), SYMTSTAT_OK);
	funct_symbol_t sym_func3;
	assert_int_equal(funct_find_symbol(table, "func3", &sym_func3), SYMTSTAT_OK);

	assert_uint_equal(table->def_arr_count, 3);
	assert_uint_equal(table->def_arr_im_count, 3);
	assert_false(table->state_insert);

	assert_int_equal(sym_func1.def_param_type_array[0], SYMT_TYPE_STRING);
	assert_int_equal(sym_func1.param_count, 1);
	assert_int_equal(sym_func1.ret_type, SYMT_TYPE_DOUBLE);

	assert_int_equal(sym_func2.def_param_type_array[0], SYMT_TYPE_DOUBLE);
	assert_int_equal(sym_func2.param_count, 1);
	assert_int_equal(sym_func2.ret_type, SYMT_TYPE_UNIT);

	assert_int_equal(sym_func3.def_param_type_array[0], SYMT_TYPE_INTEGER);
	assert_int_equal(sym_func3.param_count, 1);
	assert_int_equal(sym_func3.ret_type, SYMT_TYPE_BOOLEAN);

	funct_free(table);
}

static void funct_insert_defs_param_count_conflict(void **state) {
	(void)state;
	funct_t *table;
	funct_init(&table);

	funct_insert_begin(table, IS_FUNC_DEF);
	assert_int_equal(funct_insert_end(table, "func1", SYMT_TYPE_BOOLEAN), SYMTSTAT_OK); /* param_count = 0 */
	funct_symbol_t sym_func1;
	funct_find_symbol(table, "func1", &sym_func1);
	const int prev_param_count = sym_func1.param_count;

	funct_insert_begin(table, IS_FUNC_DEF);
	funct_insert_add_param(table, SYMT_TYPE_STRING); /* param_count = 1 */
	assert_int_equal(funct_insert_end(table, "func1", SYMT_TYPE_BOOLEAN), SYMTSTAT_FUNC_BAD_PARAM_CNT);

	assert_false(table->state_insert);
	funct_find_symbol(table, "func1", &sym_func1);
	assert_int_equal(prev_param_count, sym_func1.param_count);

	funct_free(table);
}

static void funct_insert_defs_signature_conflict(void **state) {
	(void)state;
	funct_t *table;
	funct_init(&table);

	funct_insert_begin(table, IS_FUNC_DEF);
	funct_insert_add_param(table, SYMT_TYPE_DOUBLE); /* 1:DOUBLE */
	assert_int_equal(funct_insert_end(table, "func1", SYMT_TYPE_BOOLEAN), SYMTSTAT_OK);
	funct_symbol_t sym_func1;
	funct_find_symbol(table, "func1", &sym_func1);
	const symt_data_type_t prev_param_type = sym_func1.def_param_type_array[0];

	funct_insert_begin(table, IS_FUNC_DEF);
	funct_insert_add_param(table, SYMT_TYPE_STRING); /* 1:STRING */
	assert_int_equal(funct_insert_end(table, "func1", SYMT_TYPE_BOOLEAN), SYMTSTAT_FUNC_DIFF_DEF_SIGN);

	assert_false(table->state_insert);
	funct_find_symbol(table, "func1", &sym_func1);
	assert_int_equal(prev_param_type, sym_func1.def_param_type_array[0]);

	funct_free(table);
}

static void funct_insert_defs_invalid_return(void **state) {
	(void)state;
	funct_t *table;
	funct_init(&table);

	funct_insert_begin(table, IS_FUNC_DEF);
	funct_insert_add_param(table, SYMT_TYPE_DOUBLE);
	assert_int_equal(funct_insert_end(table, "func1", SYMT_TYPE_UNDEFINED), SYMTSTAT_FUNC_BAD_RETURN);
	funct_symbol_t sym_func1;
	assert_int_equal(funct_find_symbol(table, "func1", &sym_func1), SYMTSTAT_NO_SYMBOL);

	funct_free(table);
}

static void funct_insert_defs_redefinition(void **state) {
	(void)state;
	funct_t *table;
	funct_init(&table);

	funct_insert_begin(table, IS_FUNC_DEF);
	assert_int_equal(funct_insert_end(table, "func", SYMT_TYPE_BOOLEAN), SYMTSTAT_OK);

	funct_insert_begin(table, IS_FUNC_DEF);
	assert_int_equal(funct_insert_end(table, "func", SYMT_TYPE_BOOLEAN), SYMTSTAT_BAD_ARG);

	funct_free(table);
}

/*======================================
  Advanced funct_insert_* tests combined
  ======================================*/
static void funct_insert_combined_unique(void **state) {
	(void)state;
	funct_t *table;
	funct_init(&table);

	funct_insert_begin(table, IS_FUNC_CALL);
	assert_int_equal(funct_insert_end(table, "func_call", SYMT_TYPE_UNDEFINED), SYMTSTAT_OK);

	funct_insert_begin(table, IS_FUNC_DEF);
	funct_insert_add_param(table, SYMT_TYPE_STRING);
	assert_int_equal(funct_insert_end(table, "func_def", SYMT_TYPE_BOOLEAN), SYMTSTAT_OK);

	funct_symbol_t sym_call;
	funct_symbol_t sym_def;
	assert_int_equal(funct_find_symbol(table, "func_call", &sym_call), SYMTSTAT_OK);
	assert_int_equal(funct_find_symbol(table, "func_def", &sym_def), SYMTSTAT_OK);

	assert_null(sym_call.def_param_type_array);
	assert_int_equal(sym_call.param_count, 0);
	assert_null(sym_def.call_param_type_array);
	assert_int_equal(sym_def.param_count, 1);

	funct_free(table);
}

static void funct_insert_combined_update_call(void **state) {
	(void)state;
	funct_t *table;
	funct_init(&table);

	/* Add a function call symbol (definition array not defined) */
	funct_insert_begin(table, IS_FUNC_CALL);
	funct_insert_add_param(table, SYMT_TYPE_DOUBLE);
	assert_int_equal(funct_insert_end(table, "func", SYMT_TYPE_BOOLEAN), SYMTSTAT_OK);

	/* Update the existing symbol with a definition (this must match the call signature, otherwise SHTF) */
	funct_insert_begin(table, IS_FUNC_DEF);
	funct_insert_add_param(table, SYMT_TYPE_DOUBLE);
	assert_int_equal(funct_insert_end(table, "func", SYMT_TYPE_BOOLEAN), SYMTSTAT_OK);

	funct_symbol_t sym_func;
	assert_int_equal(funct_find_symbol(table, "func", &sym_func), SYMTSTAT_OK);
	assert_non_null(sym_func.def_param_type_array);
	assert_memory_equal(sym_func.def_param_type_array, sym_func.call_param_type_array, 1);

	funct_free(table);
}

static void funct_insert_combined_update_def(void **state) {
	(void)state;
	funct_t *table;
	funct_init(&table);

	/* Add a function definition symbol (call array not defined) */
	funct_insert_begin(table, IS_FUNC_DEF);
	funct_insert_add_param(table, SYMT_TYPE_STRING);
	funct_insert_add_param(table, SYMT_TYPE_UNIT);
	assert_int_equal(funct_insert_end(table, "func", SYMT_TYPE_DOUBLE), SYMTSTAT_OK);

	/* Update the existing symbol with a call */
	funct_insert_begin(table, IS_FUNC_CALL);
	funct_insert_add_param(table, SYMT_TYPE_STRING);
	funct_insert_add_param(table, SYMT_TYPE_UNIT);
	assert_int_equal(funct_insert_end(table, "func", SYMT_TYPE_DOUBLE), SYMTSTAT_OK);

	funct_symbol_t sym_func;
	assert_int_equal(funct_find_symbol(table, "func", &sym_func), SYMTSTAT_OK);
	assert_non_null(sym_func.call_param_type_array);

	assert_memory_equal(sym_func.def_param_type_array, sym_func.call_param_type_array, 2);
	
	funct_free(table);
}

// TODO: I should add stress tests but I'm currently in a state which could only be described as "mental
// anguish", we'll just pray that these are enough for now.

/* The funny */
int main(void) {
	int test_results = 0;

	struct CMUnitTest tests_funct_init[] = {
	    cmocka_unit_test(funct_init_returns),
	    cmocka_unit_test(funct_init_single),
	};
	struct CMUnitTest tests_funct_free[] = {
	    cmocka_unit_test(funct_free_returns),
	    cmocka_unit_test(funct_free_single),
	};

	EXEC_TEST_CATEGORY("funct_init", tests_funct_init);
	EXEC_TEST_CATEGORY("funct_free", tests_funct_free);

	struct CMUnitTest tests_funct_resize_type_array[] = {
	    cmocka_unit_test(funct_resize_type_array_returns),
	    cmocka_unit_test(funct_resize_type_array_null_allocate),
	};
	struct CMUnitTest tests_funct_append_def_type[] = {
	    cmocka_unit_test(funct_append_def_type_returns),
	    cmocka_unit_test(funct_append_def_type_single),
	    cmocka_unit_test(funct_append_def_type_resize),
	};
	struct CMUnitTest tests_funct_append_call_type[] = {
	    cmocka_unit_test(funct_append_call_type_returns),
	    cmocka_unit_test(funct_append_call_type_single),
	    cmocka_unit_test(funct_append_call_type_resize),
	};
	EXEC_TEST_CATEGORY("funct_resize_type_array", tests_funct_resize_type_array);
	EXEC_TEST_CATEGORY("funct_append_def_type", tests_funct_append_def_type);
	EXEC_TEST_CATEGORY("funct_append_call_type", tests_funct_append_call_type);

	struct CMUnitTest tests_funct_compare_sym_signatures[] = {
	    cmocka_unit_test(funct_compare_sym_signatures_returns),
	    cmocka_unit_test(funct_compare_sym_signatures_by_call_sign),
	    cmocka_unit_test(funct_compare_sym_signatures_by_def_sign),
	};
	struct CMUnitTest tests_funct_copy_update_symbol[] = {
	    cmocka_unit_test(funct_copy_update_symbol_returns),
	    cmocka_unit_test(funct_copy_update_symbol_field_sequence),
	};
	EXEC_TEST_CATEGORY("funct_compare_sym_signatures", tests_funct_compare_sym_signatures);
	EXEC_TEST_CATEGORY("funct_copy_update_symbol", tests_funct_copy_update_symbol);

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

	EXEC_TEST_CATEGORY("funct_insert_begin", tests_funct_insert_begin);
	EXEC_TEST_CATEGORY("funct_insert_add_param", tests_funct_insert_add_param);
	EXEC_TEST_CATEGORY("funct_insert_end", tests_funct_insert_end);

	struct CMUnitTest tests_funct_find_symbol[] = {
	    cmocka_unit_test(funct_find_symbol_returns),
	    cmocka_unit_test(funct_find_symbol_empty_table),
	};
	EXEC_TEST_CATEGORY("funct_find_symbol", tests_funct_find_symbol);

	struct CMUnitTest tests_funct_nth_param_type[] = {
	    cmocka_unit_test(funct_nth_param_type_returns),
	    cmocka_unit_test(funct_nth_param_type_parameterless_symbol),
	    cmocka_unit_test(funct_nth_param_type_def_precedence),
	};
	EXEC_TEST_CATEGORY("func_nth_param_type", tests_funct_nth_param_type);

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

	EXEC_TEST_CATEGORY("funct_pending_insert", tests_funct_pending_insert);
	EXEC_TEST_CATEGORY("funct_exists", tests_funct_exists);

	struct CMUnitTest tests_adv_funct_insert_call[] = {
	    cmocka_unit_test(funct_insert_begin_state_reset),
	    cmocka_unit_test(funct_insert_calls_unique),
	    cmocka_unit_test(funct_insert_calls_param_count_conflict),
	    cmocka_unit_test(funct_insert_calls_return_conflict),
	    cmocka_unit_test(funct_insert_calls_signature_conflict),
	    cmocka_unit_test(funct_insert_calls_update_existing),
	};
	EXEC_TEST_CATEGORY("Advanced funct_insert_* w/ call symbol type", tests_adv_funct_insert_call);

	struct CMUnitTest tests_adv_funct_insert_def[] = {
	    cmocka_unit_test(funct_insert_defs_unique),
	    cmocka_unit_test(funct_insert_defs_param_count_conflict),
	    cmocka_unit_test(funct_insert_defs_signature_conflict),
	    cmocka_unit_test(funct_insert_defs_invalid_return),
	    cmocka_unit_test(funct_insert_defs_redefinition),
	};
	EXEC_TEST_CATEGORY("Advanced funct_insert_* w/ def symbol type", tests_adv_funct_insert_def);

	struct CMUnitTest tests_adv_funct_insert_combined[] = {
	    cmocka_unit_test(funct_insert_combined_unique),
	    cmocka_unit_test(funct_insert_combined_update_call),
	    cmocka_unit_test(funct_insert_combined_update_def),
	};
	EXEC_TEST_CATEGORY("Advanced funct_insert_* w/ combined symbols (def & call)",
	                   tests_adv_funct_insert_combined);
	
	return test_results;
}
