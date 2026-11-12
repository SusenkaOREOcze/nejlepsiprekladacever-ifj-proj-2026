/**
   @file functable_impl.h
   @author xbosakd00 (DasBean125)
   @date 2026-10-25

   @brief Private interface for the function table module.
*/

#ifndef FUNCTABLE_IMPL_H
#define FUNCTABLE_IMPL_H

#include "functable.h"
#include "hashtable.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

/*==========
  Data types
  ==========*/

DEFTYPE_HASHTABLE(funct_symbol_t) funct_search_table_t;

/**
   The table of functions, but this time there's only one level.
*/
struct Functable {
	/**
	   An array of types for every function definition in the function table (see @ref funct_symbol_t).
	*/
	int8_t *def_type_array;

	/**
	   An array of types for every function call in the function table (see @ref funct_symbol_t).
	*/
	int8_t *call_type_array;

	size_t def_arr_size;
	size_t def_arr_count;    //!< The first free index in the definition array.
	size_t def_arr_im_count; //!< Imaginary first index - elements between this and *_count may be invalid.

	size_t call_arr_size;
	size_t call_arr_count;    //!< The first free index in the call array.
	size_t call_arr_im_count; //!< Imaginary first index - elements between this and *_count may be invalid.

	bool state_insert;             //!< true if a symbol insertion is in progress, false otherwise.
	bool is_insert_definition;     //!< true if the inserted symbol is a function definition
	funct_symbol_t pending_symbol; //!< Symbol in the making. Invalid if state_insert == false.

	/**
	   A single, global function scope (namespace). It's a search table, okay?
	*/
	funct_search_table_t scope;
};

/*=========
  Functions
  =========*/

/**
   Resize a type array from the funct_t structure.

   @param array A pointer to the address of the array to change the size of.
   @param new_size The new size of the array.
   @returns 0 on success or 1 on failure.
*/
int funct_resize_type_array(int8_t **const array, const size_t new_size);

/**
   Append a parameter type to a definition array, potentially resizing it.

   @param table The function table containing the array.
   @param type The type of the parameter at the given index.
   @returns 0 on success, 1 on failure.
*/
int funct_append_def_type(funct_t *table, const symt_data_type_t type);

/**
   Append a parameter type to a call array, potentially resizing it.

   @param table The function table containing the array.
   @param type The type of the parameter at the given index.
   @returns 0 on success, 1 on failure.
*/
int funct_append_call_type(funct_t *table, const symt_data_type_t type);

/**
   Compare the function signature of two symbols. Shortest parameter array match first.

   @param table The table which contains both compared symbols.
   @param base_sym The symbol to compare to.
   @param comp_sym The symbol to compare.
   @returns SYMTSTAT_OK if symbol signatures match, SYMTSTAT_FUNC_DIFF_CALL_SIGN if compared symbol's
   signatures differ from base call sign. and base def signature doesn't exist, SYMTSTAT_FUNC_DIFF_DEF_SIGN if
   compared symbol's signatures differ from base def signature or SYMTSTAT_BAD_ARG if called with
   uninitialized symbols or bad arguments.
*/
symt_stat_t funct_compare_sym_signatures(const funct_t *table, const funct_symbol_t *base_sym,
                                         const funct_symbol_t *comp_sym);

/**
   Update a function symbol.

   Depending on if the source symbol is a definition or a call symbol, the destination symbol is updated or
   overwritten respectively.

   @param dst_sym The destination symbol (the symbol to update).
   @param src_sym The source symbol (the symbol to draw data from).
   @param is_def Specifies if the source symbol is a definition symbol.
   @returns SYMTSTAT_OK on success or SYMTSTAT_BAD_ARG when the arguments don't make sense.
*/
symt_stat_t funct_copy_update_symbol(funct_symbol_t *dst_sym, const funct_symbol_t *src_sym,
                                     const bool is_def);

/**
   A predicate for asserting if the given symbol has a definition signature or not.

   @param sym The symbol to test.
   @returns true if the symbol has a valid definition signature, or false if not or if the argument is bad.
*/
inline bool funct_sym_has_def_sign(const funct_symbol_t *sym) {
	if (sym == NULL)
		return false;
	return sym->has_def_sign && sym->param_count >= 0 && sym->def_array_offset >= 0;
}

/**
   A predicate for asserting if the given symbol signed the contract that states "Thou shalt have an
   indisputably valid call signature".

   @param sym The symbol to test.
   @returns true if the symbol has a valid call signature, or false if not or if the argument is bad.
*/
inline bool funct_sym_has_call_sign(const funct_symbol_t *sym) {
	if (sym == NULL)
		return false;
	return sym->has_call_sign && sym->param_count >= 0 && sym->call_array_offset >= 0;
}

#ifndef UNIT_UNDER_TEST

/* I beg you to compile with -O2 */
extern inline bool funct_sym_has_def_sign(const funct_symbol_t *sym);
extern inline bool funct_sym_has_call_sign(const funct_symbol_t *sym);

#endif /* UNIT_UNDER_TEST not defined */

#endif
