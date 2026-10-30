/**
   @file functable.c
   @author xbosakd00 (DasBean125)
   @date 2026-10-25

   @brief A module defining the function table data structure.
*/

#include "include/functable.h"
#include "include/functable_impl.h"

/*=========
  CONSTANTS
  =========*/

constexpr size_t FUNCT_INIT_TYPE_ARRAY_SIZE = 1024;
constexpr size_t FUNCT_INIT_SCOPE_SIZE = 128;

static constexpr funct_symbol_t EMPTY_SYMBOL = {
    .call_param_type_array = NULL,
    .def_param_type_array = NULL,
    .param_count = -1,
    .ret_type = SYMT_TYPE_UNDEFINED,
};

/*================
  HELPER FUNCTIONS
  ================*/

int funct_resize_type_array(int8_t **const array, const size_t new_size) {
	if (array == NULL)
		return 1;

	/* Change memory size of the current array, potentially moving it to a different address */
	int8_t *new_array = realloc(*array, new_size * sizeof(*new_array));
	if (new_array == NULL)
		return 1;

	*array = new_array;
	return 0;
}

int funct_append_def_type(funct_t *table, const symt_data_type_t type) {
	if (table == NULL)
		return 1;

	/* No need to resize yet */
	if (table->def_arr_im_count >= table->def_arr_size) {
		if (funct_resize_type_array(&table->def_type_array, table->def_arr_size * 2))
			return 1;
	}

	/* Append the device (note how it uses the "imaginary" index) */
	table->def_type_array[table->def_arr_im_count] = type;
	table->def_arr_im_count++;
	return 0;
}

int funct_append_call_type(funct_t *table, const symt_data_type_t type) {
	if (table == NULL)
		return 1;

	/* No need to resize yet */
	if (table->call_arr_im_count >= table->call_arr_size) {
		if (funct_resize_type_array(&table->call_type_array, table->call_arr_size * 2))
			return 1;
	}

	/* Append the device (note how it uses the "imaginary" index) */
	table->call_type_array[table->call_arr_im_count] = type;
	table->call_arr_im_count++;
	return 0;
}

symt_stat_t funct_compare_sym_signatures(const funct_symbol_t *base_sym, const funct_symbol_t *comp_sym) {
	if (base_sym == NULL || comp_sym == NULL)
		return SYMTSTAT_BAD_ARG;
	/* Nothing to compare */
	if (base_sym->def_param_type_array == NULL && base_sym->call_param_type_array == NULL)
		return SYMTSTAT_BAD_ARG;
	if (comp_sym->def_param_type_array == NULL && comp_sym->call_param_type_array == NULL)
		return SYMTSTAT_BAD_ARG;

	/* Note how this doesn't check parameter count identity (you have to do that yourself) */
	int min_param_count =
	    (base_sym->param_count > comp_sym->param_count ? comp_sym->param_count : base_sym->param_count);
	if (min_param_count == -1) /* Undefined symbol */
		return SYMTSTAT_BAD_ARG;

	/* No base definition signature - compare to call signature */
	if (base_sym->def_param_type_array == NULL)
		for (int i = 0; i < min_param_count; i++) {
			symt_stat_t base_call_param = base_sym->call_param_type_array[i];
			/* This works because of the NULL check at the beginning of the function */
			symt_stat_t comp_param =
			    (comp_sym->def_param_type_array != NULL ? comp_sym->def_param_type_array[i]
				                                        : comp_sym->call_param_type_array[i]);
			if (base_call_param != comp_param)
				return SYMTSTAT_FUNC_DIFF_CALL_SIGN;
		}
	/* Base has definition signature - compare to it */
	else
		for (int i = 0; i < min_param_count; i++) {
			symt_stat_t base_def_param = base_sym->def_param_type_array[i];
			/* This works because of the NULL check at the beginning of the function */
			symt_stat_t comp_param =
			    (comp_sym->def_param_type_array != NULL ? comp_sym->def_param_type_array[i]
				                                        : comp_sym->call_param_type_array[i]);
			if (base_def_param != comp_param)
				return SYMTSTAT_FUNC_DIFF_DEF_SIGN;
		}

	return SYMTSTAT_OK;
}

symt_stat_t funct_copy_update_symbol(funct_symbol_t *dst_sym, const funct_symbol_t *src_sym) {
	if (dst_sym == NULL || src_sym == NULL)
		return SYMTSTAT_BAD_ARG;

	/* Update def/call arrays if undefined */
	if (dst_sym->def_param_type_array == NULL)
		dst_sym->def_param_type_array = src_sym->def_param_type_array;
	if (dst_sym->call_param_type_array == NULL)
		dst_sym->call_param_type_array = src_sym->call_param_type_array;

	/* Update return type if undefined */
	if (dst_sym->ret_type == SYMT_TYPE_UNDEFINED)
		dst_sym->ret_type = src_sym->ret_type;

	return SYMTSTAT_OK;
}

/*=============
  INIT AND FREE
  =============*/

symt_stat_t funct_init(funct_t **const func_table) {
	if (func_table == NULL)
		return SYMTSTAT_BAD_ARG;

	/* Table allocation */
	funct_t *new_table = malloc(sizeof(*new_table));
	if (new_table == NULL)
		return SYMTSTAT_BAD_ALLOC;

	/* Type array allocations */
	new_table->call_type_array = malloc(FUNCT_INIT_TYPE_ARRAY_SIZE * sizeof(int8_t));
	if (new_table->call_type_array == NULL)
		goto FAIL_ALLOC_CALL;

	new_table->def_type_array = malloc(FUNCT_INIT_TYPE_ARRAY_SIZE * sizeof(int8_t));
	if (new_table->def_type_array == NULL)
		goto FAIL_ALLOC_DEF;

	/* Hashtable (scope, search table, whatever you fancy) init */
	if (HTBL_INIT(new_table->scope, FUNCT_INIT_SCOPE_SIZE) != HTBLSTAT_OK)
		goto FAIL_ALLOC_HTBL;

	/* Remaining table init */
	new_table->def_arr_count = 0;
	new_table->def_arr_im_count = 0;
	new_table->def_arr_size = FUNCT_INIT_TYPE_ARRAY_SIZE;

	new_table->call_arr_count = 0;
	new_table->call_arr_im_count = 0;
	new_table->call_arr_size = FUNCT_INIT_TYPE_ARRAY_SIZE;

	new_table->state_insert = false;
	new_table->pending_symbol = EMPTY_SYMBOL;
	*func_table = new_table;

	return SYMTSTAT_OK;

	/* Sequential free for every successfully allocated piece of memory */
FAIL_ALLOC_HTBL:
	free(new_table->def_type_array);
FAIL_ALLOC_DEF:
	free(new_table->call_type_array);
FAIL_ALLOC_CALL:
	free(new_table);

	return SYMTSTAT_BAD_ALLOC;
}

symt_stat_t funct_free(funct_t *func_table) {
	if (func_table == NULL)
		return SYMTSTAT_BAD_ARG;

	/* Blow it all up */
	free(func_table->call_type_array);
	free(func_table->def_type_array);
	HTBL_FREE(func_table->scope);
	free(func_table);

	return SYMTSTAT_OK;
}

/*===================================
  PREDICATES, FIND SYMBOL & UTILITIES
  ===================================*/

symt_stat_t funct_find_symbol(const funct_t *table, const char *ident, funct_symbol_t *out_symbol) {
	if (table == NULL || ident == NULL || out_symbol == NULL)
		return SYMTSTAT_BAD_ARG;

	/* Starting to realize these are just glorified wrapper functions... */
	if (HTBL_RETRIEVE(table->scope, ident, *out_symbol) != HTBLSTAT_OK)
		return SYMTSTAT_NO_SYMBOL;

	return SYMTSTAT_OK;
}

symt_stat_t funct_nth_param_type(const funct_symbol_t *func_symbol, const int param_number,
                                 symt_data_type_t *out_type) {
	if (func_symbol == NULL || out_type == NULL)
		return SYMTSTAT_BAD_ARG;
	if (param_number >= func_symbol->param_count)
		return func_symbol->param_count < 0 ? SYMTSTAT_FUNC_NO_PARAMS : SYMTSTAT_BAD_ARG;

	/* Definition parameters have priority */
	if (func_symbol->def_param_type_array != NULL)
		*out_type = func_symbol->def_param_type_array[param_number];
	else
		*out_type = func_symbol->call_param_type_array[param_number];

	return SYMTSTAT_OK;
}

bool funct_pending_insert(const funct_t *table) {
	if (table == NULL)
		return false;

	return table->state_insert;
}

bool funct_exists(const funct_t *table, const char *ident) {
	if (table == NULL || ident == NULL)
		return false;

	return HTBL_EXISTS(table->scope, ident);
}

/*================
  SYMBOL INSERTION
  ================*/

symt_stat_t funct_insert_begin(funct_t *table) {
	if (table == NULL)
		return SYMTSTAT_BAD_ARG;
	if (table->state_insert)
		return SYMTSTAT_BAD_STATE;

	/* Start with an empty slate */
	table->pending_symbol = EMPTY_SYMBOL;
	table->state_insert = true;
	return SYMTSTAT_OK;
}

symt_stat_t funct_insert_add_param(funct_t *table, const symt_data_type_t param_type, const bool is_def) {
	if (table == NULL)
		return SYMTSTAT_BAD_ARG;
	if (!table->state_insert)
		return SYMTSTAT_BAD_STATE;

	table->pending_symbol.param_count++;

	/* Function definition symbol */
	if (is_def) {
		if (table->pending_symbol.def_param_type_array == NULL)
			table->pending_symbol.def_param_type_array = table->def_type_array + table->def_arr_count;

		if (funct_append_call_type(table, param_type))
			return SYMTSTAT_BAD_ALLOC;
		table->def_arr_im_count++;
		return SYMTSTAT_OK;
	}

	/* Function call symbol */
	if (table->pending_symbol.call_param_type_array == NULL)
		table->pending_symbol.call_param_type_array = table->call_type_array + table->call_arr_count;

	if (funct_append_def_type(table, param_type))
		return SYMTSTAT_BAD_ALLOC;
	table->call_arr_im_count++;
	return SYMTSTAT_OK;
}

symt_stat_t funct_insert_end(funct_t *table, const char *ident, const symt_data_type_t ret,
                             const bool is_def) {
	if (table == NULL || ident == NULL)
		return SYMTSTAT_BAD_ARG;
	if (!table->state_insert)
		return SYMTSTAT_BAD_STATE;

	funct_symbol_t existing_sym = {};
	funct_symbol_t *pending_sym = &table->pending_symbol;
	symt_stat_t result = SYMTSTAT_OK;

	/* Nothing was retrieved - new symbol :) */
	if (!HTBL_RETRIEVE(table->scope, ident, existing_sym)) {
		pending_sym->ret_type = ret;
		if (HTBL_INSERT(table->scope, ident, &table->pending_symbol) != HTBLSTAT_OK)
			return SYMTSTAT_BAD_ALLOC; /* This _doesn't_ end the insert state */

		table->state_insert = false;
		return SYMTSTAT_OK;
	} /* Not a new symbol - check everything :( */

	/* Wrong parameter count */
	if (existing_sym.param_count != pending_sym->param_count) {
		result = SYMTSTAT_FUNC_BAD_PARAM_CNT;
		goto EXIT_RESULT;
	}
	/* Wrong signatures */
	if ((result = funct_compare_sym_signatures(&existing_sym, pending_sym)) != SYMTSTAT_OK)
		goto EXIT_RESULT;

	if (is_def) { /* Function definition */
		          /* Definition already exists (redefinition is not allowed) */
		if (existing_sym.def_param_type_array != NULL) {
			result = SYMTSTAT_BAD_ARG;
			goto EXIT_RESULT;
		}

		/* Undefined return type or mismatched return types */
		if (pending_sym->ret_type == SYMT_TYPE_UNDEFINED || existing_sym.ret_type != pending_sym->ret_type) {
			result = SYMTSTAT_FUNC_BAD_RETURN;
			goto EXIT_RESULT;
		}
	} else { /* Function call */
		     /* Previous call has a defined return type which differs from pending symbol */
		if (existing_sym.ret_type != SYMT_TYPE_UNDEFINED && existing_sym.ret_type != pending_sym->ret_type) {
			result = SYMTSTAT_FUNC_BAD_RETURN;
			goto EXIT_RESULT;
		}
	}

EXIT_RESULT:
	funct_copy_update_symbol(&existing_sym, pending_sym);
	if (HTBL_INSERT(table->scope, ident, &existing_sym) != HTBLSTAT_OK)
		return SYMTSTAT_BAD_ALLOC; /* This _doesn't_ end the insert state */

	table->state_insert = false;
	return result;
}
