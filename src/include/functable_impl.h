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
	   Stored as 8bit integers to save on memory.
	*/
	int8_t *func_def_type_array;

	/**
	   An array of types for every function call in the function table (see @ref funct_symbol_t).
	*/
	int8_t *func_call_type_array;

	size_t def_array_size;
	size_t def_array_count;    //!< The first free index in the definition array.
	size_t def_array_im_count; //!< Imaginary first index - elements between this and *_count may be invalid.

	size_t call_array_size;
	size_t call_array_count;    //!< The first free index in the call array.
	size_t call_array_im_count; //!< Imaginary first index - elements between this and *_count may be invalid.

	bool state_insert;          //!< If true, then an insertion of a symbol is pending.

	/**
	   A single, global function scope (namespace). Again, essentially just a search table.
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
   @returns 0 on success, 1 on failure or 2 if the operation resulted in nothing (new_size == old_size).
*/
int funct_resize_type_array(int8_t **const array, const size_t new_size);

#endif
