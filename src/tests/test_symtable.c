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

/* FIXME: these tests must run after symt_init/free/insert/scope_depth tests */
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

/* FIXME: old, pls fix :-) */
void test_allocation(void **state) {
	(void)state;

	print_message("  |-symt_init()\n");
	symt_t *table;
	assert_uint_equal(symt_init(NULL), SYMTSTAT_BAD_ARG);
	assert_uint_equal(symt_init(&table), SYMTSTAT_OK);

	assert_uint_equal(table->uid_accum, 0);
	assert_uint_equal(table->scope_depth, 0);
	assert_non_null(table->scope_top);
	assert_non_null(table->scope_top->scope.table);

	symt_scope_t *aux_standby_top = table->standby_top;
	size_t standby_count = 0;
	while (aux_standby_top != NULL) {
		assert_non_null(aux_standby_top);
		assert_non_null(aux_standby_top->scope.table);
		aux_standby_top = aux_standby_top->prev;
		standby_count++;
	}
	assert_uint_equal(standby_count, 10);

	print_message("  \\-symt_free()\n");
	assert_uint_equal(symt_free(NULL), SYMTSTAT_BAD_ARG);
	assert_uint_equal(symt_free(table), SYMTSTAT_OK); /* table is invalid after this point */
}

void test_scope_depth_changing(void **state) {
	(void)state;

	symt_t *table;
	symt_init(&table);

	symt_scope_t *const old_global_scope = table->scope_top;
	symt_scope_t *const old_standby_scope = table->standby_top;
	symt_scope_t *const old_prev_standby_scope = table->standby_top->prev;

	print_message("  |-symt_inc_scope_depth()\n");
	assert_uint_equal(symt_inc_scope_depth(NULL), SYMTSTAT_BAD_ARG);
	assert_uint_equal(symt_inc_scope_depth(table), SYMTSTAT_OK);

	assert_uint_equal(table->scope_depth, 1);
	assert_ptr_equal(old_global_scope, table->scope_top->prev);   /* Current scope has moved down by one */
	assert_ptr_equal(old_standby_scope, table->scope_top);        /* New scope is the first standby scope */
	assert_ptr_equal(old_prev_standby_scope, table->standby_top); /* Top of standby matches second-to-top
	                                                               * standby scope */

	print_message("  |-symt_dec_scope_depth()\n");
	assert_uint_equal(symt_dec_scope_depth(NULL), SYMTSTAT_BAD_ARG);
	assert_uint_equal(symt_dec_scope_depth(table), SYMTSTAT_OK);

	assert_uint_equal(table->scope_depth, 0);
	assert_uint_equal(old_global_scope, table->scope_top);
	assert_uint_equal(old_standby_scope, table->standby_top);
	assert_uint_equal(old_prev_standby_scope, table->standby_top->prev);

	assert_uint_equal(symt_dec_scope_depth(table), SYMTSTAT_OK);
	assert_uint_equal(table->scope_depth, 0);
	assert_non_null(table->scope_top);

	print_message("  \\-new standby scope allocation\n");
	for (size_t i = 0; i < 16; i++) {
		assert_uint_equal(symt_inc_scope_depth(table), SYMTSTAT_OK);
		assert_non_null(table->standby_top); /* Once this reaches the 11th iteration and onward, this
		                                      * better not be NULL */
	}

	symt_free(table);
}

void test_symbol_io_single_scope(void **state) {
	(void)state;

	symt_t *bad_table = malloc(sizeof(*bad_table));
	bad_table->scope_top = NULL;

	symt_t *table;
	symt_init(&table);
	symt_symbol_t new_symbol = {
	    .mutable = true,
	    .data_type = SYMT_TYPE_DOUBLE,
	};
	const unsigned long long old_uid_accum = table->uid_accum;

	print_message("  |-symt_insert()\n");
	assert_uint_equal(symt_insert(NULL, "hi", &new_symbol), SYMTSTAT_BAD_ARG);
	assert_uint_equal(symt_insert(table, NULL, &new_symbol), SYMTSTAT_BAD_ARG);
	assert_uint_equal(symt_insert(table, "hi", NULL), SYMTSTAT_BAD_ARG);
	assert_uint_equal(symt_insert(bad_table, "nuh uh", &new_symbol), SYMTSTAT_NO_SCOPE);

	assert_uint_equal(symt_insert(table, "myVariable", &new_symbol), SYMTSTAT_OK);
	assert_uint_equal(table->uid_accum, old_uid_accum + 1);

	new_symbol.mutable = false;
	assert_uint_equal(symt_insert(table, "myValue", &new_symbol), SYMTSTAT_OK);
	assert_uint_equal(table->uid_accum, old_uid_accum + 2);

	print_message("  \\-symt_find_symbol()\n");
	const unsigned long long special_uid = 42;
	symt_symbol_t retr_symbol = {
	    .uid = special_uid,
	};
	assert_uint_equal(symt_find_symbol(NULL, "hi", &retr_symbol), SYMTSTAT_BAD_ARG);
	assert_uint_equal(symt_find_symbol(bad_table, "hi", &retr_symbol), SYMTSTAT_NO_SYMBOL);
	assert_uint_equal(retr_symbol.uid, special_uid);
	assert_uint_equal(symt_find_symbol(table, NULL, &retr_symbol), SYMTSTAT_BAD_ARG);
	assert_uint_equal(symt_find_symbol(table, "hi", NULL), SYMTSTAT_BAD_ARG);

	assert_uint_equal(symt_find_symbol(table, "nonexistent", &retr_symbol), SYMTSTAT_NO_SYMBOL);
	assert_uint_equal(retr_symbol.uid, special_uid);

	assert_uint_equal(symt_find_symbol(table, "myVariable", &retr_symbol), SYMTSTAT_OK);
	assert_true(retr_symbol.mutable);
	assert_int_equal(retr_symbol.data_type, SYMT_TYPE_DOUBLE);
	assert_uint_equal(retr_symbol.uid, 0);

	assert_uint_equal(symt_find_symbol(table, "myValue", &retr_symbol), SYMTSTAT_OK);
	assert_false(retr_symbol.mutable);
	assert_int_equal(retr_symbol.data_type, SYMT_TYPE_DOUBLE);
	assert_uint_equal(retr_symbol.uid, 1);

	symt_free(table);
	free(bad_table);
}

void test_symbol_io_shadowing(void **state) {
	(void)state;
	symt_t *table;
	symt_init(&table);

	symt_symbol_t new_symbol = {
	    .data_type = SYMT_TYPE_STRING,
	    .mutable = true,
	};
	symt_insert(table, "global_symbol", &new_symbol);  /* uid == 0 */
	symt_insert(table, "global2_symbol", &new_symbol); /* uid == 1 */
	symt_inc_scope_depth(table);
	symt_insert(table, "local1_symbol", &new_symbol); /* uid == 2 */
	symt_insert(table, "global_symbol", &new_symbol); /* uid == 3 */

	symt_symbol_t retr_symbol;
	assert_uint_equal(symt_find_symbol(table, "global_symbol", &retr_symbol), SYMTSTAT_OK);
	assert_uint_equal(retr_symbol.uid, 3);
	assert_uint_equal(symt_find_symbol(table, "global2_symbol", &retr_symbol), SYMTSTAT_OK);
	assert_uint_equal(retr_symbol.uid, 1);
	assert_uint_equal(symt_find_symbol(table, "local1_symbol", &retr_symbol), SYMTSTAT_OK);
	assert_uint_equal(retr_symbol.uid, 2);

	symt_dec_scope_depth(table);
	assert_uint_equal(symt_find_symbol(table, "local1_symbol", &retr_symbol), SYMTSTAT_NO_SYMBOL);
	assert_uint_equal(symt_find_symbol(table, "global_symbol", &retr_symbol), SYMTSTAT_OK);
	assert_uint_equal(retr_symbol.uid, 0); /* note the difference between this and line 160 */
	assert_uint_equal(symt_find_symbol(table, "global2_symbol", &retr_symbol), SYMTSTAT_OK);
	assert_uint_equal(retr_symbol.uid, 1);

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

	/* TODO: ADD symtable interaction tests */

	struct CMUnitTest symt_search_tests[] = {
	    cmocka_unit_test(symt_exists_in_cs_returns),
	    cmocka_unit_test(symt_exists_in_cs_several_global_scope),
	    cmocka_unit_test(symt_exists_in_cs_several_nested_scope),
	};
	EXEC_TEST_CATEGORY("Symtable scope searching", symt_search_tests);

	free(valid_table);
	return test_results;
}
