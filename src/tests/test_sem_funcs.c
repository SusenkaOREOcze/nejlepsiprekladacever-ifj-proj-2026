/**
	@file test_sem_funcs.c
	@brief CMocka tests of built-in functions and call checking (sem_funcs.c).
 */

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include "cmocka/cmocka.h"

#include "../include/sem_funcs.h"

#define I SYMT_TYPE_INTEGER
#define D SYMT_TYPE_DOUBLE
#define S SYMT_TYPE_STRING
#define U SYMT_TYPE_UNIT
#define B SYMT_TYPE_BOOLEAN

#define TYPES(...) ((symt_data_type_t[]){__VA_ARGS__})

static funct_t *functions;

/* The table contains one user function:  def f(a: Int, b: Double): String */
static int setup(void **state) {
	(void)state;
	assert_int_equal(funct_init(&functions), SYMTSTAT_OK);
	assert_int_equal(funct_insert_begin(functions, IS_FUNC_DEF), SYMTSTAT_OK);
	assert_int_equal(funct_insert_add_param(functions, I), SYMTSTAT_OK);
	assert_int_equal(funct_insert_add_param(functions, D), SYMTSTAT_OK);
	assert_int_equal(funct_insert_end(functions, "f", S), SYMTSTAT_OK);
	return 0;
}

static int teardown(void **state) {
	(void)state;
	funct_free(functions);
	return 0;
}

static sem_result_t call(const char *name, int arg_count, const symt_data_type_t *arg_types, symt_data_type_t *ret_type) {
	return sem_check_call(functions, name, arg_count, arg_types, ret_type);
}

static void builtin_lookup(void **state) {
	(void)state;
	assert_non_null(sem_find_builtin("print"));
	assert_non_null(sem_find_builtin("subStr"));
	assert_non_null(sem_find_builtin("readBoolean"));
	assert_null(sem_find_builtin("substr"));
	assert_null(sem_find_builtin("f"));
	assert_null(sem_find_builtin(NULL));
}

static void builtin_calls(void **state) {
	(void)state;
	symt_data_type_t ret = SYMT_TYPE_UNDEFINED;

	assert_int_equal(call("print", 1, TYPES(I), &ret), SEM_OK);
	assert_int_equal(ret, U);
	assert_int_equal(call("print", 1, TYPES(U), &ret), SEM_OK); /* Any accepts Unit */
	assert_int_equal(call("print", 1, TYPES(B), &ret), SEM_OK);
	assert_int_equal(call("print", 0, NULL, &ret), SEM_ERR_ARGUMENTS);
	assert_int_equal(call("print", 2, TYPES(I, I), &ret), SEM_ERR_ARGUMENTS);

	assert_int_equal(call("readInt", 0, NULL, &ret), SEM_OK);
	assert_int_equal(ret, I);
	assert_int_equal(call("readBoolean", 0, NULL, &ret), SEM_OK);
	assert_int_equal(ret, B);
	assert_int_equal(call("toInt", 1, TYPES(D), &ret), SEM_OK);
	assert_int_equal(ret, I);
	assert_int_equal(call("toInt", 1, TYPES(I), &ret), SEM_OK); /* Int is widened to Double */
	assert_int_equal(call("toInt", 1, TYPES(S), &ret), SEM_ERR_ARGUMENTS);

	assert_int_equal(call("subStr", 3, TYPES(S, I, I), &ret), SEM_OK);
	assert_int_equal(ret, S);
	assert_int_equal(call("subStr", 3, TYPES(S, D, I), &ret), SEM_ERR_ARGUMENTS);
	assert_int_equal(call("chr", 1, TYPES(S), &ret), SEM_ERR_ARGUMENTS);
}

static void user_function_calls(void **state) {
	(void)state;
	symt_data_type_t ret = SYMT_TYPE_UNDEFINED;

	assert_int_equal(call("f", 2, TYPES(I, D), &ret), SEM_OK);
	assert_int_equal(ret, S);
	assert_int_equal(call("f", 2, TYPES(I, I), &ret), SEM_OK); /* Int is widened to Double Trouble */
	assert_int_equal(call("f", 2, TYPES(D, D), &ret), SEM_ERR_ARGUMENTS);
	assert_int_equal(call("f", 1, TYPES(I), &ret), SEM_ERR_ARGUMENTS);
	assert_int_equal(call("f", 3, TYPES(I, D, I), &ret), SEM_ERR_ARGUMENTS);
	assert_int_equal(call("g", 0, NULL, &ret), SEM_ERR_UNDEFINED);
}

static void parameter_types(void **state) {
	(void)state;
	assert_int_equal(sem_param_type(functions, "f", 0), I);
	assert_int_equal(sem_param_type(functions, "f", 1), D);
	assert_int_equal(sem_param_type(functions, "f", 2), SEM_TYPE_ANY);
	assert_int_equal(sem_param_type(functions, "ord", 1), I);
	assert_int_equal(sem_param_type(functions, "toStr", 0), SEM_TYPE_ANY);
	assert_int_equal(sem_param_type(functions, "unknown", 0), SEM_TYPE_ANY);
}

int main(void) {
	const struct CMUnitTest tests[] = {
	    cmocka_unit_test_setup_teardown(builtin_lookup, setup, teardown),
	    cmocka_unit_test_setup_teardown(builtin_calls, setup, teardown),
	    cmocka_unit_test_setup_teardown(user_function_calls, setup, teardown),
	    cmocka_unit_test_setup_teardown(parameter_types, setup, teardown),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
