/**
   @file symtable.h
   @author xbosakd00 (DasBean125)
   @date 2026-10-17

   @brief Public interface definition for the symbol table data structure.

   Why did you implement the hashtable as a separate, generic abstract data type, I hear you ask? Crack. The
   answer is crack.
*/

#ifndef SYMTABLE_H
#define SYMTABLE_H

#include <stddef.h>
#include <stdint.h>

/*==========
  DATA TYPES
  ==========*/

/**
   A set of status codes pertinent to the symbol table data structure.

   You're an excellent programmer, so you'll probably only need the first code from this list.
*/
typedef enum SymtableStatusCode {
	SYMTSTAT_OK = 0,
	SYMTSTAT_BAD_ARG,
	SYMTSTAT_BAD_ALLOC,
	SYMTSTAT_NO_SYMBOL,
	SYMTSTAT_NO_SCOPE,
} symt_stat_t;

/**
   Identifies what kind of data a symbol shall hold.
*/
typedef enum SymtableDataType {
	SYMT_TYPE_UNIT,
	SYMT_TYPE_INTEGER,
	SYMT_TYPE_DOUBLE,
	SYMT_TYPE_STRING,
	SYMT_TYPE_BOOLEAN,
} symt_data_type_t;

/**
   A symbol from the symbol table. How symbolic.

   @note The UID field is defined because it is possible to have several instances of a variable with the same
   name across different scopes with the same depth. In case you were wondering, the UID can hold 2^64 - 1, or
   1.84467440737e+19 values. Good luck making that many symbols.
*/
typedef struct SymtableSymbol {
	symt_data_type_t data_type;
	bool mutable; //!< States if this is symbol is of the IFJ26 storage class var (true) or val (false).
	unsigned long long uid; //!< Unique identfier of this specific symbol.
} symt_symbol_t;

/**
   The actual symbol table type. This is _just_ a single table, trust (thousand-yard stare).

   @note This is a state machine, i.e. every Symtable struct initialized by symt_init() has an internal state
   that changes as you work with it.
*/
typedef struct Symtable symt_t;

/*====================
  DATA TYPE OPERATIONS
  ====================*/

/**
   Initialize a symbol table in memory. The scope depth is initialized to 0 (global).

   @param[out] sym_table A pointer to a symbol table variable.
   @returns SYMTSTAT_OK on success, an error code otherwise.
*/
symt_stat_t symt_init(symt_t *sym_table);

/**
   Rid the memory of a symbol table.

   @param[out] sym_table The symbol table to destroy.
   @returns SYMTSTAT_OK on success, an error code otherwise.
*/
symt_stat_t symt_free(symt_t *sym_table);

/**
   Increment the current scope depth.

   @note If this function returns SYMTSTAT_BAD_ALLOC, the scope depth is still increased.

   @param sym_table The symbol table to work with.
   @returns SYMTSTAT_OK on success, an error code otherwise.
*/
symt_stat_t symt_inc_scope_depth(symt_t *sym_table);

/**
   Decrement the current scope depth.

   @note If the current scope depth is 0 (global scope), this function does nothing.

   @param sym_table The symbol table to work with.
   @returns SYMTSTAT_OK on success, an error code otherwise.
*/
symt_stat_t symt_dec_scope_depth(symt_t *sym_table);

/**
   Insert an identifier-symbol pair into a symbol table.

   @note The semantics of this operation vary based on the current state. If a symbol is not defined in the
   current scope, a new one is inserted; otherwise the existing symbol is overwritten. Because it is possible
   that a symbol shares a name with that from another scope (or even from a different scope of the same
   depth), the UID of the symbol is set automatically even if you manually specify it.

   @param sym_table The symbol table to insert into.
   @param ident The identifier (string) of the symbol.
   @param[in] symbol A structure containing the symbol's data.
   @returns SYMTSTAT_OK on success, an error otherwise.
*/
symt_stat_t symt_insert(symt_t *sym_table, const char *ident, symt_symbol_t *symbol);

/**
   Find a symbol with the given identifier in a symbol table and return its data.

   @param sym_table The table to search through.
   @param ident The symbol identifier to look for.
   @param[out] out_symbol A pointer to a variable that will receive the symbol data. If this function fails,
   this variable will remain unmodified.
   @returns SYMTSTAT_OK on success, SYMTSTAT_NO_SYMBOL if the identifier has no symbol assigned, or an error.
*/
symt_stat_t symt_find_symbol(symt_t *sym_table, const char *ident, symt_symbol_t *out_symbol);

#endif
