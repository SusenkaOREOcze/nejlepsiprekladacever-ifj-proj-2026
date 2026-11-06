/**
   @file test_symtable.c
   @author xbosakd00 (DasBean125)
   @data 2026-10-19

   @brief Tests the symtable table data structure. Your life definitely depends on if this succeeds.

   Be patient.
*/

#define UNIT_UNDER_TEST
#include "../include/symtable.h"
#include "../include/symtable_impl.h"

#include "cmocka/cmocka.h"
#include <string.h>

#define EXEC_TEST_CATEGORY(name, tests)                                                                      \
	do {                                                                                                     \
		printf("\n[    " name "    ]\n");                                                                    \
		test_results |= (cmocka_run_group_tests_name((name), (tests), NULL, NULL));                          \
	} while (0)

#define ASSERT_SYM_EQ(src, dst)                                                                              \
	do {                                                                                                     \
		assert_int_equal((src).mutable, (dst).mutable);                                                      \
		assert_int_equal((src).data_type, (dst).data_type);                                                  \
	} while (0)

static symt_t *valid_table = NULL;

/*=====================
  Helper function tests
  =====================*/
static void symt_alloc_scope_returns(void **state) {
	(void)state;
	symt_scope_t *scope = NULL;

	assert_int_equal(symt_alloc_scope(NULL, 32), SYMTSTAT_BAD_ARG);
	assert_int_equal(symt_alloc_scope(&scope, 0), SYMTSTAT_BAD_ARG);
}

static void symt_alloc_scope_single(void **state) {
	(void)state;

	symt_scope_t *scope = NULL;
	assert_int_equal(symt_alloc_scope(&scope, 14), SYMTSTAT_OK);
	assert_null(scope->prev);

	HTBL_FREE(scope->scope);
	free(scope);
}

static void symt_free_scope_test(void **state) {
	(void)state;

	symt_scope_t *scope = NULL;
	symt_alloc_scope(&scope, 13);

	symt_free_scope(scope); /* address sanitizer should catch the failure here */
}

static void symt_push_scope_returns(void **state) {
	(void)state;
	symt_scope_t *scope = NULL;
	assert_int_equal(symt_push_scope(NULL, scope), SYMTSTAT_BAD_ARG);
	assert_int_equal(symt_push_scope(&scope, scope), SYMTSTAT_BAD_ARG); /* horrifying but it works */
}

static void symt_push_scope_empty_stack(void **state) {
	(void)state;
	symt_scope_t *scope = NULL;

	symt_scope_t *new_scope = NULL;
	symt_alloc_scope(&new_scope, 20);

	assert_int_equal(symt_push_scope(&scope, new_scope), SYMTSTAT_OK);
	assert_ptr_equal(scope, new_scope);
	assert_null(scope->prev);

	symt_free_scope(scope);
}

static void symt_push_scope_full_stack(void **state) {
	(void)state;
	symt_scope_t *scope = NULL;
	symt_alloc_scope(&scope, 156); /* main scope */
	symt_scope_t *const prev_top = scope;

	symt_scope_t *new_scope = NULL;
	symt_alloc_scope(&new_scope, 20);

	assert_int_equal(symt_push_scope(&scope, new_scope), SYMTSTAT_OK);
	assert_ptr_equal(scope, new_scope);
	assert_non_null(scope->prev);
	assert_ptr_equal(scope->prev, prev_top);

	symt_free_scope(scope->prev);
	symt_free_scope(scope);
}

static void symt_pop_scope_returns(void **state) {
	(void)state;

	symt_scope_t *scope_top = NULL;
	symt_scope_t *scope = NULL;

	assert_int_equal(symt_pop_scope(NULL, &scope), SYMTSTAT_BAD_ARG);
	assert_int_equal(symt_pop_scope(&scope_top, NULL), SYMTSTAT_BAD_ARG);
	assert_int_equal(symt_pop_scope(&scope_top, &scope), SYMTSTAT_NO_SCOPE);
}

static void symt_pop_scope_full_stack(void **state) {
	(void)state;
	symt_scope_t *scope_top = NULL;
	symt_alloc_scope(&scope_top, 42);
	symt_scope_t *const old_top = scope_top;

	symt_scope_t *scope = NULL;
	symt_alloc_scope(&scope, 42);
	symt_scope_t *const old_local = scope;
	symt_push_scope(&scope_top, scope);

	scope = NULL;
	assert_int_equal(symt_pop_scope(&scope_top, &scope),
	                 SYMTSTAT_OK); /* scope will contain the local scope (last pushed scope) */

	assert_non_null(scope);
	assert_null(scope->prev);
	assert_ptr_equal(old_local, scope); /* we got the same scope that we pushed */

	assert_ptr_equal(old_top, scope_top); /* the scope bellow the popped one is the global (top) scope */
	assert_null(scope_top->prev);

	symt_free_scope(scope);
	symt_free_scope(scope_top);
}

static void symt_clear_scope_test(void **state) {
	(void)state;
	symt_symbol_t symbol = {
	    .data_type = SYMT_TYPE_UNIT,
	    .mutable = false,
	};

	assert_int_equal(symt_clear_scope(NULL), SYMTSTAT_BAD_ARG);
	symt_scope_t *scope_top = NULL;
	symt_alloc_scope(&scope_top, 42);
	HTBL_INSERT(scope_top->scope, "I know", &symbol);
	HTBL_INSERT(scope_top->scope, "the pieces", &symbol);
	HTBL_INSERT(scope_top->scope, "FIT", &symbol);

	/* This should probably go in a separate test, but I cba */
	assert_int_equal(symt_clear_scope(scope_top), SYMTSTAT_OK);
	assert_false(HTBL_EXISTS(scope_top->scope, "I know"));
	assert_false(HTBL_EXISTS(scope_top->scope, "the pieces"));
	assert_false(HTBL_EXISTS(scope_top->scope, "FIT"));

	symt_free_scope(scope_top);
}

/*==========================
  symt_init, symt_free tests
  ==========================*/
static void symt_init_test(void **state) {
	(void)state;
	symt_t *table;

	assert_int_equal(symt_init(NULL), SYMTSTAT_BAD_ARG);
	assert_int_equal(symt_init(&table), SYMTSTAT_OK);

	assert_uint_equal(table->scope_depth, 0);
	assert_uint_equal(table->uid_accum, 0);
	assert_non_null(table->scope_top);
	assert_non_null(table->standby_top);

	const size_t desired_scope_count = 1;
	size_t actual_scope_count = 0;
	const size_t desired_standby_count = 10;
	size_t actual_standby_count = 0;

	/* cleanup */
	while (table->scope_top) {
		symt_scope_t *scope = table->scope_top;
		table->scope_top = scope->prev;
		HTBL_FREE(scope->scope);
		free(scope);
		actual_scope_count++;
	}
	assert_uint_equal(desired_scope_count, actual_scope_count);

	while (table->standby_top) {
		symt_scope_t *scope = table->standby_top;
		table->standby_top = scope->prev;
		HTBL_FREE(scope->scope);
		free(scope);
		actual_standby_count++;
	}
	assert_uint_equal(desired_standby_count, actual_standby_count);
	
	free(table);
}

static void symt_free_test(void **state) {
	(void)state;
	symt_t *table;

	assert_int_equal(symt_free(NULL), SYMTSTAT_BAD_ARG);
	symt_init(&table);

	assert_int_equal(symt_free(table), SYMTSTAT_OK);
}

/*============================
  Scope depth adjustment tests
  ============================*/
static void symt_inc_scope_depth_returns(void **state) {
	(void)state;
	symt_t table = {
	    .scope_top = NULL,
	    .standby_top = NULL,
	    .scope_depth = 0,
	    .uid_accum = 0,
	};

	assert_int_equal(symt_inc_scope_depth(NULL), SYMTSTAT_BAD_ARG);
	assert_int_equal(symt_inc_scope_depth(&table), SYMTSTAT_NO_SCOPE);
}

static void symt_inc_scope_depth_single(void **state) {
	(void)state;
	symt_t *table;
	symt_init(&table);

	const symt_scope_t *old_scope_top = table->scope_top;
	const symt_scope_t *old_standby_top = table->standby_top;

	assert_int_equal(symt_inc_scope_depth(table), SYMTSTAT_OK);
	assert_uint_equal(table->scope_depth, 1);
	assert_uint_equal(table->uid_accum, 0);
	assert_ptr_equal(table->scope_top, old_standby_top);
	assert_ptr_equal(table->scope_top->prev, old_scope_top);
	assert_ptr_not_equal(table->standby_top, old_standby_top);

	symt_free(table);
}

static void symt_inc_scope_depth_consecutive(void **state) {
	(void)state;
	symt_t *table;
	symt_init(&table);

	symt_scope_t *old_scope_top = table->scope_top;
	symt_scope_t *old_standby_top = table->standby_top;

	for (int i = 0; i < 9; i++) {
		/* allocation occurs if standby_top is NULL in _this_ call */
		assert_int_equal(symt_inc_scope_depth(table), SYMTSTAT_OK);
		assert_ptr_equal(old_standby_top, table->scope_top);
		assert_ptr_equal(old_scope_top, table->scope_top->prev);

		old_scope_top = table->scope_top;
		old_standby_top = table->standby_top;
	}
	assert_uint_equal(table->scope_depth, 9);
	assert_non_null(table->standby_top);

	symt_free(table);
}

static void symt_inc_scope_depth_allocation(void **state) {
	(void)state;
	symt_t *table;
	symt_init(&table);

	for (int i = 0; i < 9; i++)
		assert_int_equal(symt_inc_scope_depth(table), SYMTSTAT_OK);

	const symt_scope_t *old_scope_top = table->scope_top;
	assert_int_equal(symt_inc_scope_depth(table), SYMTSTAT_OK);

	assert_uint_equal(table->scope_depth, 10);
	assert_ptr_not_equal(old_scope_top, table->scope_top);
	assert_ptr_equal(old_scope_top, table->scope_top->prev);
	assert_non_null(table->standby_top);

	symt_free(table);
}

static void symt_dec_scope_depth_returns(void **state) {
	(void)state;
	symt_t table = {
	    .scope_depth = 0,
	    .scope_top = NULL,
	    .standby_top = NULL,
	    .uid_accum = 0,
	};

	assert_int_equal(symt_dec_scope_depth(NULL), SYMTSTAT_BAD_ARG);
	assert_int_equal(symt_dec_scope_depth(&table), SYMTSTAT_BAD_ARG); /* uninitialized table, should fail */
}

static void symt_dec_scope_depth_single(void **state) {
	(void)state;
	symt_t *table;
	symt_init(&table);

	symt_inc_scope_depth(table);
	const symt_scope_t *old_scope_top = table->scope_top;
	const symt_scope_t *old_standby_top = table->standby_top;
	const symt_scope_t *old_second_scope_top = table->scope_top->prev;

	assert_int_equal(symt_dec_scope_depth(table), SYMTSTAT_OK);
	assert_uint_equal(table->scope_depth, 0);
	assert_uint_equal(table->uid_accum, 0);
	assert_ptr_equal(table->scope_top, old_second_scope_top);
	assert_null(table->scope_top->prev);
	assert_ptr_equal(table->standby_top, old_scope_top);
	assert_ptr_equal(table->standby_top->prev, old_standby_top);

	/* testing decrementing on a global scope */
	assert_int_equal(symt_dec_scope_depth(table), SYMTSTAT_OK);
	assert_uint_equal(table->scope_depth, 0);
	assert_non_null(table->scope_top);
	assert_non_null(table->standby_top);

	symt_free(table);
}

/*===================================
  Symbol table symbol insertion tests
  ===================================*/
static void symt_insert_returns(void **state) {
	(void)state;
	symt_t *table = (void *)0xDEADBEEF; /* I love C */
	symt_symbol_t sym = {
	    .mutable = false,
	    .data_type = SYMT_TYPE_DOUBLE,
	};

	symt_t uninit_table = {
	    .scope_depth = 0,
	    .scope_top = NULL,
	    .standby_top = NULL,
	    .uid_accum = 0,
	};

	assert_int_equal(symt_insert(NULL, "angels on the", &sym), SYMTSTAT_BAD_ARG);
	assert_int_equal(symt_insert(table, NULL, &sym), SYMTSTAT_BAD_ARG);
	assert_int_equal(symt_insert(table, "", &sym), SYMTSTAT_BAD_ARG);
	assert_int_equal(symt_insert(table, "sideline again", NULL), SYMTSTAT_BAD_ARG);

	assert_int_equal(symt_insert(&uninit_table, "betting on their patience and reason", &sym),
	                 SYMTSTAT_NO_SCOPE);
}

static void symt_insert_single(void **state) {
	(void)state;
	symt_t *table;
	symt_init(&table);
	symt_symbol_t sym = {
	    .mutable = true,
	    .data_type = SYMT_TYPE_INTEGER,
	    .uid = -1,
	};

	assert_int_equal(symt_insert(table, "clutch it like a cornerstone", &sym), SYMTSTAT_OK);
	assert_uint_equal(table->uid_accum, 1);

	symt_symbol_t out_sym;
	assert_true(HTBL_RETRIEVE(table->scope_top->scope, "clutch it like a cornerstone", out_sym));
	assert_int_equal(sym.mutable, out_sym.mutable);
	assert_int_equal(sym.data_type, out_sym.data_type);
	assert_uint_equal(out_sym.uid, 0);

	symt_free(table);
}

static void symt_insert_nested_scope(void **state) {
	(void)state;
	symt_t *table;
	symt_symbol_t sym = {
	    .mutable = true,
	    .data_type = SYMT_TYPE_STRING,
	};

	symt_init(&table);
	assert_int_equal(symt_insert(table, "EMD", &sym), SYMTSTAT_OK);
	const symt_scope_t *global_scope = table->scope_top;

	symt_inc_scope_depth(table);
	assert_int_equal(symt_insert(table, "12-567C", &sym), SYMTSTAT_OK);
	assert_false(HTBL_EXISTS(table->scope_top->scope, "EMD"));
	assert_true(HTBL_EXISTS(table->scope_top->scope, "12-567C"));

	assert_false(HTBL_EXISTS(global_scope->scope, "12-567C"));
	assert_true(HTBL_EXISTS(global_scope->scope, "EMD"));

	symt_free(table);
}

static void symt_dec_scope_depth_standby_cleanup(void **state) {
	(void)state;
	symt_t *table;
	symt_init(&table);

	symt_inc_scope_depth(table);
	symt_symbol_t sym = {
	    .mutable = false,
	    .data_type = SYMT_TYPE_UNIT,
	};
	symt_insert(table, "here we have a stone", &sym);

	assert_int_equal(symt_dec_scope_depth(table), SYMTSTAT_OK);
	assert_false(HTBL_EXISTS(table->standby_top->scope, "here we have a stone"));
	assert_false(HTBL_EXISTS(table->scope_top->scope, "here we have a stone"));

	symt_free(table);
}

/*=====================
  Symbol searching tests
  =====================*/
static void symt_exists_in_cs_returns(void **state) {
	(void)state;

	assert_false(symt_exists_in_current_scope(NULL, "hi"));
	assert_false(symt_exists_in_current_scope(valid_table, NULL));
	assert_false(symt_exists_in_current_scope(valid_table, ""));
}

static void symt_exists_in_cs_several_global_scope(void **state) {
	(void)state;
	symt_t *table;
	symt_init(&table);
	symt_symbol_t symbol = {
	    .mutable = true,
	    .data_type = SYMT_TYPE_INTEGER,
	};
	assert_uint_equal(table->scope_depth, 0);

	symt_insert(table, "prkl0", &symbol);
	symt_insert(table, "prkl1", &symbol);
	symt_insert(table, "prkl2", &symbol);

	assert_true(symt_exists_in_current_scope(table, "prkl0"));
	assert_true(symt_exists_in_current_scope(table, "prkl1"));
	assert_true(symt_exists_in_current_scope(table, "prkl2"));
	assert_false(symt_exists_in_current_scope(table, "prkl3"));

	symt_free(table);
}

static void symt_exists_in_cs_several_nested_scope(void **state) {
	(void)state;

	symt_t *table;
	symt_init(&table);
	symt_symbol_t symbol = {
	    .mutable = true,
	    .data_type = SYMT_TYPE_INTEGER,
	};
	assert_uint_equal(table->scope_depth, 0); /* global symbols */
	symt_insert(table, "global0", &symbol);
	symt_insert(table, "global1", &symbol);

	symt_inc_scope_depth(table);
	assert_uint_equal(table->scope_depth, 1); /* nested scope 1 symbols */
	symt_insert(table, "local0", &symbol);
	symt_insert(table, "local1", &symbol);

	assert_true(symt_exists_in_current_scope(table, "local0"));
	assert_true(symt_exists_in_current_scope(table, "local1"));
	assert_false(symt_exists_in_current_scope(table, "local2"));

	assert_false(symt_exists_in_current_scope(table, "global0"));
	assert_false(symt_exists_in_current_scope(table, "global1"));

	symt_free(table);
}

static void symt_find_symbol_returns(void **state) {
	(void)state;
	symt_t *table = (void *)0xDEADBEEF;
	symt_symbol_t sym = {
	    .mutable = false,
	    .data_type = SYMT_TYPE_STRING,
	};

	assert_int_equal(symt_find_symbol(NULL, "hi", &sym), SYMTSTAT_BAD_ARG);
	assert_int_equal(symt_find_symbol(table, NULL, &sym), SYMTSTAT_BAD_ARG);
	assert_int_equal(symt_find_symbol(table, "hi", NULL), SYMTSTAT_BAD_ARG);
	assert_int_equal(symt_find_symbol(table, "", &sym), SYMTSTAT_BAD_ARG);
}

static void symt_find_symbol_single_scope(void **state) {
	(void)state;
	symt_t *table;
	symt_init(&table);
	symt_symbol_t sym = {
	    .mutable = true,
	    .data_type = SYMT_TYPE_BOOLEAN,
	};
	symt_symbol_t sym2 = {
	    .mutable = false,
	    .data_type = SYMT_TYPE_STRING,
	};

	symt_insert(table, "withering my", &sym);
	symt_insert(table, "intuition", &sym2);

	symt_symbol_t out_sym;
	assert_int_equal(symt_find_symbol(table, "withering my", &out_sym), SYMTSTAT_OK);
	ASSERT_SYM_EQ(sym, out_sym);

	assert_int_equal(symt_find_symbol(table, "intuition", &out_sym), SYMTSTAT_OK);
	ASSERT_SYM_EQ(sym2, out_sym);

	assert_int_equal(symt_find_symbol(table, "nope", &out_sym), SYMTSTAT_NO_SYMBOL);

	symt_free(table);
}

static void symt_find_symbol_multi_scope(void **state) {
	(void)state;
	symt_t *table;
	symt_init(&table);
	symt_symbol_t sym = {
	    .mutable = true,
	    .data_type = SYMT_TYPE_BOOLEAN,
	};
	symt_symbol_t sym2 = {
	    .mutable = false,
	    .data_type = SYMT_TYPE_STRING,
	};

	symt_insert(table, "I know", &sym);
	symt_inc_scope_depth(table);
	symt_insert(table, "it's stupid", &sym2);

	symt_symbol_t out_sym;
	assert_int_equal(symt_find_symbol(table, "I know", &out_sym), SYMTSTAT_OK);
	ASSERT_SYM_EQ(sym, out_sym);

	assert_int_equal(symt_find_symbol(table, "it's stupid", &out_sym), SYMTSTAT_OK);
	ASSERT_SYM_EQ(sym2, out_sym);

	symt_free(table);
}

static void symt_find_symbol_shadowing(void **state) {
	(void)state;
	symt_t *table;
	symt_init(&table);
	symt_symbol_t sym = {
	    .mutable = true,
	    .data_type = SYMT_TYPE_BOOLEAN,
	};
	symt_symbol_t sym2 = {
	    .mutable = false,
	    .data_type = SYMT_TYPE_STRING,
	};

	symt_insert(table, "the same", &sym);
	symt_inc_scope_depth(table);
	symt_insert(table, "the same", &sym2);

	symt_symbol_t out_sym;
	assert_int_equal(symt_find_symbol(table, "the same", &out_sym), SYMTSTAT_OK);
	ASSERT_SYM_EQ(sym2, out_sym); /* local scope priority */

	symt_free(table);
}

int main(void) {
	int test_results = 0;
	valid_table = malloc(sizeof(*valid_table));
	valid_table->scope_top = NULL;
	valid_table->standby_top = NULL;
	valid_table->uid_accum = 0;
	valid_table->scope_depth = 0;

	struct CMUnitTest helper_func_tests[] = {
	    cmocka_unit_test(symt_alloc_scope_returns),    cmocka_unit_test(symt_alloc_scope_single),
	    cmocka_unit_test(symt_free_scope_test),        cmocka_unit_test(symt_push_scope_returns),
	    cmocka_unit_test(symt_push_scope_empty_stack), cmocka_unit_test(symt_push_scope_full_stack),
	    cmocka_unit_test(symt_pop_scope_returns),      cmocka_unit_test(symt_pop_scope_full_stack),
	    cmocka_unit_test(symt_clear_scope_test),
	};

	EXEC_TEST_CATEGORY("Symtable helper functions", helper_func_tests);

	struct CMUnitTest symt_basic_interaction_tests[] = {
	    cmocka_unit_test(symt_init_test),
	    cmocka_unit_test(symt_free_test),
	    cmocka_unit_test(symt_inc_scope_depth_returns),
	    cmocka_unit_test(symt_inc_scope_depth_single),
	    cmocka_unit_test(symt_inc_scope_depth_consecutive),
	    cmocka_unit_test(symt_inc_scope_depth_allocation),
	    cmocka_unit_test(symt_dec_scope_depth_returns),
	    cmocka_unit_test(symt_dec_scope_depth_single),
	};
	EXEC_TEST_CATEGORY("Symtable basic interaction functions", symt_basic_interaction_tests);

	struct CMUnitTest symt_symbol_insertion_tests[] = {
	    cmocka_unit_test(symt_insert_returns),
	    cmocka_unit_test(symt_insert_single),
	    cmocka_unit_test(symt_insert_nested_scope),
	    cmocka_unit_test(symt_dec_scope_depth_standby_cleanup),
	};
	EXEC_TEST_CATEGORY("Symtable symbol insertion functions", symt_symbol_insertion_tests);

	struct CMUnitTest symt_search_tests[] = {
	    cmocka_unit_test(symt_exists_in_cs_returns),
	    cmocka_unit_test(symt_exists_in_cs_several_global_scope),
	    cmocka_unit_test(symt_exists_in_cs_several_nested_scope),
	    cmocka_unit_test(symt_find_symbol_returns),
	    cmocka_unit_test(symt_find_symbol_single_scope),
	    cmocka_unit_test(symt_find_symbol_multi_scope),
	    cmocka_unit_test(symt_find_symbol_shadowing),
	};
	EXEC_TEST_CATEGORY("Symtable symbol search functions", symt_search_tests);

	free(valid_table);
	return test_results;
}
