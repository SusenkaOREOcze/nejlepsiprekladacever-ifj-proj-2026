/**
   @file symtable.h

   @author xbosakd00 (DasBean125)

   @date 2026-10-17

   @brief Public interface definition for the symbol table data structure.

   Why did you implement the hashtable as a separate, generic abstract data type, I hear you ask? Crack. The
   answer is crack (legal notice: the previous two statements are sarcasm).
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

   They're intent on telling you exactly how wrong (or well) something went when you called a symtable
   function, but you're an excellent programmer, so you'll probably only need the first code from this list.
*/
typedef enum SymtableStatusCode {
	SYMTSTAT_OK = 0,
	SYMTSTAT_BAD_ARG,
	SYMTSTAT_BAD_ALLOC,
	SYMTSTAT_NO_SYMBOL,
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
   The internal representation of the IFJ26 'Int' data type.
*/
typedef int32_t ifj26_int_t;
/**
   The internal representation of the IFJ26 'Double' data type.
*/
typedef double ifj26_double_t;
/**
   The internal representation of the IFJ26 'Boolean' data type.
*/
typedef bool ifj26_boolean_t;
/**
   The internal representation of the IFJ26 'String' data type.
*/
typedef const char *ifj26_string_t;
/**
   The internal representation of the IFJ26 'Unit' data type.
*/
typedef void *const ifj26_unit_t;

/**
   Holds one unique value of an IFJ26 data type.

   Now before you start thinking about OOP, no, this is not that. There was a time when computers knew what
   data they were working with. A time when the programmer could build code that took advantage of the
   problem. Then came SOLID and many other OOP paradigms... Please tell me about your optimized code that has
   to call four functions scattered across the entire memory heap to figure out where the data it's _going to
   work with_ (it hasn't even started yet) is.
*/
typedef union SymtableDataObject {
	ifj26_int_t v_integer;
	ifj26_double_t v_double;
	ifj26_boolean_t v_boolean;
	ifj26_string_t v_string;
	ifj26_unit_t v_unit;
} symt_data_obj_t;

#define SYMT_FLAG_MUTABLE true
#define SYMT_FLAG_IMMUTABLE false

/**
   A symbol from the symbol table.

   @note Depending on the value of .data_type, only one field of .data_value will contain meaningful data.
   Since C99, accessing the data stored in a union through a different member than the last one used to store
   data in said union will reinterpret that data as the type of the accessed member, which is called 'type
   punning'. All of this is to say that xbosakd00 is not responsible for any misinterpretation of data caused
   by improper use of the union.
*/
typedef struct SymtableSymbol {
	symt_data_type_t data_type;
	symt_data_obj_t data_value;
	bool mutable; //!< States if this is symbol is of the IFJ26 storage class var (true) or val (false).
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
   Initialize a symbol table in memory.

   @param sym_table A pointer to a symbol table variable.

   @returns SYMTSTAT_OK on success, an error code otherwise.
*/
symt_stat_t symt_init(symt_t *sym_table);

/**
   Rid the memory of a symbol table.

   @param sym_table The symbol table to destroy.

   @returns SYMTSTAT_OK on success, an error code otherwise.
*/
symt_stat_t symt_free(symt_t *sym_table);

/**
   Set the current scope depth to the desired value.

   This alters the semantics of the symt_insert_* operations.

   @param sym_table The symbol table to work with.

   @param depth The desired scope depth. "Global" scope has depth 0.

   @returns SYMTSTAT_OK on success, an error code otherwise.
*/
symt_stat_t symt_set_scope_depth(symt_t *sym_table, size_t depth);

/**
   Increment the current scope depth.

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
   Insert an identifier of the IFJ26 type 'Unit' into a symbol table.

   @param sym_table The symbol table to insert into.

   @param ident The identifier (string) of the symbol.

   @param flags Sets additional data about the symbol.

   @returns SYMTSTAT_OK on success, an error otherwise.
*/
#define symt_insert_unit(sym_table, ident, flags) symtable_insert_unit(&(sym_table), (ident), (flags))

/**
   Insert an identifier of the IFJ26 type 'Int' into a symbol table.

   @note This function casts the 'value' argument to the appropriate internal IFJ26 type representation. See
   ifj26_* typedefs for more info.

   @param sym_table The symbol table to insert into.

   @param ident The identifier (string) of the symbol.

   @param flags Sets additional data about the symbol.

   @param value The initial value (if any) of the symbol.

   @returns SYMTSTAT_OK on success, an error otherwise.
*/
#define symt_insert_int(sym_table, ident, flags, value)                                                      \
	symtable_insert_int(&(sym_table), (ident), (flags), (ifj26_int_t)(value))

/**
   Insert an identifier of the IFJ26 type 'Double' into a symbol table. Also @see symt_insert_int().

   @param sym_table The symbol table to insert into.

   @param ident The identifier (string) of the symbol.

   @param flags Sets additional data about the symbol.

   @param value The initial value (if any) of the symbol.

   @returns SYMTSTAT_OK on success, an error otherwise.
*/
#define symt_insert_double(sym_table, ident, flags, value)                                                   \
	symtable_insert_double(&(sym_table), (ident), (flags), (ifj26_double_t)(value))

/**
   Insert an identifier of the IFJ26 type 'Boolean' into a symbol table. Also @see symt_insert_int().

   @param sym_table The symbol table to insert into.

   @param ident The identifier (string) of the symbol.

   @param flags Sets additional data about the symbol.

   @param value The initial value (if any) of the symbol.

   @returns SYMTSTAT_OK on success, an error otherwise.
*/
#define symt_insert_boolean(sym_table, ident, flags, value)                                                  \
	symtable_insert_boolean(&(sym_table), (ident), (flags), (ifj26_boolean_t)(value))

/**
   Insert an identifier of the IFJ26 type 'String' into a symbol table. Also @see symt_insert_int().

   @param sym_table The symbol table to insert into.

   @param ident The identifier (string) of the symbol.

   @param flags Sets additional data about the symbol.

   @param value The initial value (if any) of the symbol.

   @returns SYMTSTAT_OK on success, an error otherwise.
*/
#define symt_insert_string(sym_table, ident, flags, value)                                                   \
	symtable_insert_string(&(sym_table), (ident), (flags), (ifj26_string_t)(value))

/**
   Find a symbol with the given identifier in a symbol table and return its data.

   @param sym_table The table to search through.

   @param ident The symbol identifier to look for.

   @param out_symbol A pointer to a variable that will receive the symbol data. If this function fails, this
   variable will remain unmodified.

   @returns SYMTSTAT_OK on success, SYMTSTAT_NO_SYMBOL if the identifier has no symbol assigned, or an error.
*/
symt_stat_t symt_find_symbol(symt_t *sym_table, const char *ident, symt_symbol_t *out_symbol);

/*=========
  FUNCTIONS (these shouldn't be used by the user)
  =========*/

symt_stat_t symtable_insert_unit(symt_t *sym_table, const char *ident, bool is_mutable);

symt_stat_t symtable_insert_int(symt_t *sym_table, const char *ident, bool is_mutable, ifj26_int_t value);

symt_stat_t symtable_insert_double(symt_t *sym_table, const char *ident, bool is_mutable,
                                   ifj26_double_t value);

symt_stat_t symtable_insert_boolean(symt_t *sym_table, const char *ident, bool is_mutable,
                                    ifj26_boolean_t value);

symt_stat_t symtable_insert_string(symt_t *sym_table, const char *ident, bool is_mutable,
                                   ifj26_string_t value);

#endif
