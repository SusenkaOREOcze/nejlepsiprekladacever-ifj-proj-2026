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
	const struct CMUnitTest tests[] = {
	    cmocka_unit_test(test_allocation),
	    cmocka_unit_test(test_scope_depth_changing),
	    cmocka_unit_test(test_symbol_io_single_scope),
	    cmocka_unit_test(test_symbol_io_shadowing),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
