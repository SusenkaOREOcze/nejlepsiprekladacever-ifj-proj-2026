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
  DATA TYPES
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
	funct_symbol_t pending_symbol; //!< Symbol in the making. Invalid if state_insert == false.

	/**
	   A single, global function scope (namespace). It's a search table, okay?
	*/
	funct_search_table_t scope;
};

/*=========
  FUNCTIONS
  =========*/

/**
   Resize a type array from the funct_t structure.

   @param array A pointer to the address of the array to change the size of.
   @param new_size The new size of the array.
   @returns 0 on success, 1 on failure or 2 if this call would do nothing (new_size is identical to current).
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

#endif
