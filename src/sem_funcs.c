
/**
 	@file sem_funcs.c
	@author xmejstj00 (kmejstr)
	@brief Built-in functions of IFJ26 and checking of function calls.
 */

#include "include/sem_funcs.h"

#include <string.h>

/**
   The built-in functions from section 4 of the specification and readBoolean from BOOLTHEN.
 
   This is a fixed list of constants, not a symbol table: built-in functions are known in advance and
   their Any parameters cannot be expressed by the function table.
 */
static const sem_builtin_t BUILTINS[] = {
    {"print", 1, {SEM_TYPE_ANY}, SYMT_TYPE_UNIT},
    {"readLine",0, {}, SYMT_TYPE_STRING},
    {"readInt", 0, {}, SYMT_TYPE_INTEGER},
    {"readDouble", 0, {}, SYMT_TYPE_DOUBLE},
    {"readBoolean", 0, {}, SYMT_TYPE_BOOLEAN},
    {"toStr",  1, {SEM_TYPE_ANY}, SYMT_TYPE_STRING},
    {"toInt", 1, {SYMT_TYPE_DOUBLE}, SYMT_TYPE_INTEGER},
    {"strLength", 1, {SYMT_TYPE_STRING}, SYMT_TYPE_INTEGER},
    {"subStr", 3, {SYMT_TYPE_STRING, SYMT_TYPE_INTEGER, SYMT_TYPE_INTEGER}, SYMT_TYPE_STRING},
    {"ord", 2, {SYMT_TYPE_STRING, SYMT_TYPE_INTEGER}, SYMT_TYPE_INTEGER},
    {"chr", 1, {SYMT_TYPE_INTEGER}, SYMT_TYPE_STRING},
};

static const size_t BUILTIN_COUNT = sizeof(BUILTINS) / sizeof(BUILTINS[0]);

const sem_builtin_t *sem_find_builtin(const char *name) {
	if (name == NULL)
		return NULL;

	for (size_t i = 0; i < BUILTIN_COUNT; i++)
		if (strcmp(BUILTINS[i].name, name) == 0)
			return &BUILTINS[i];
	return NULL;
}

/**
  Checks one argument against one parameter.
 */
static bool sem_arg_matches(symt_data_type_t param, symt_data_type_t arg) {
	if (!sem_is_value_type(arg))
		return false;
	if (param == SEM_TYPE_ANY)
		return true;
	return sem_assignable(param, arg);
}

sem_result_t sem_check_args(int param_count, const symt_data_type_t *params, int arg_count,
                            const symt_data_type_t *arg_types) {
	if (arg_count != param_count)
		return SEM_ERR_ARGUMENTS;

	for (int i = 0; i < arg_count; i++)
		if (!sem_arg_matches(params[i], arg_types[i]))
			return SEM_ERR_ARGUMENTS;

	return SEM_OK;
}

sem_result_t sem_check_call(const funct_t *functions, const char *name, int arg_count,
                            const symt_data_type_t *arg_types, symt_data_type_t *out_ret_type) {
	if (name == NULL || out_ret_type == NULL || arg_count < 0 || (arg_count > 0 && arg_types == NULL))
		return SEM_ERR_INTERNAL;

	/* Built-in functions are checked against the fixed list */
	const sem_builtin_t *builtin = sem_find_builtin(name);
	if (builtin != NULL) {
		sem_result_t result = sem_check_args(builtin->param_count, builtin->params, arg_count, arg_types);
		if (result == SEM_OK)
			*out_ret_type = builtin->ret_type;
		return result;
	}

	/* User functions are checked against their definition in the function table */
	funct_symbol_t symbol;
	if (funct_find_symbol(functions, name, &symbol) != SYMTSTAT_OK || !symbol.has_def_sign)
		return SEM_ERR_UNDEFINED;

	if (arg_count != symbol.param_count)
		return SEM_ERR_ARGUMENTS;

	for (int i = 0; i < arg_count; i++) {
		symt_data_type_t param_type;
		if (funct_nth_param_type(functions, &symbol, i, &param_type) != SYMTSTAT_OK)
			return SEM_ERR_INTERNAL;
		if (!sem_arg_matches(param_type, arg_types[i]))
			return SEM_ERR_ARGUMENTS;
	}

	*out_ret_type = symbol.ret_type;
	return SEM_OK;
}

symt_data_type_t sem_param_type(const funct_t *functions, const char *name, int index) {
	if (name == NULL || index < 0)
		return SEM_TYPE_ANY;

	const sem_builtin_t *builtin = sem_find_builtin(name);
	if (builtin != NULL)
		return index < builtin->param_count ? builtin->params[index] : SEM_TYPE_ANY;

	funct_symbol_t symbol;
	symt_data_type_t param_type;
	if (funct_find_symbol(functions, name, &symbol) != SYMTSTAT_OK ||
	    funct_nth_param_type(functions, &symbol, index, &param_type) != SYMTSTAT_OK)
		return SEM_TYPE_ANY;

	return param_type;
}
