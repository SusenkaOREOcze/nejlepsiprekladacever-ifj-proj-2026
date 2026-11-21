
/**
 @file sem_funcs.h
 @author xmejtsj00 (kmejstr)
 @brief Built-in functions of IFJ26 and checking of function calls.
 */

#ifndef SEM_FUNCS_H
#define SEM_FUNCS_H

#include "functable.h"
#include "sem_types.h"

/**
  @brief Placeholder type "Any" of a built-in function parameter (pint, toStr).
  It is used only inside the semantic analysis and is never stored in a symbol table.
 */
#define SEM_TYPE_ANY SYMT_TYPE_UNDEFINED

#define SEM_BUILTIN_MAX_PARAMS 3

/**
  @brief Signature of one built-in function (section 4 of the specification, readBoolean fromBOOLTHEN).
 */
typedef struct SemBuiltin {
	const char *name;
	int param_count;
	symt_data_type_t params[SEM_BUILTIN_MAX_PARAMS];
	symt_data_type_t ret_type;
} sem_builtin_t;

/**
   @brief Finds a built-in function by its name.
   @param name Function name.
   @return The built-in function, or NULL when the name is not a built-in function.
 */
const sem_builtin_t *sem_find_builtin(const char *name);

/**
   @brief Checks the number and types of call arguments against function parameters.
   A parameter of type SEM_TYPE_ANY accepts every value type. Other parameters accept their own type and Int where Double is expected.
   @return SEM_OK or SEM_ERR_ARGUMENTS.
 */
sem_result_t sem_check_args(int param_count, const symt_data_type_t *params, int arg_count, const symt_data_type_t *arg_types);

/**
   @brief Checks a call of a built-in or user function.
   User functions are looked up in the function table, which must already contain their definitions.
   @param functions Function table with the definitions of user functions.
   @param name Name of the called function.
   @param arg_count Number of arguments.
   @param arg_types Types of the arguments, from left to right.
   @param[out] out_ret_type Receives the return type of the function on success.
   @return SEM_OK, SEM_ERR_UNDEFINED (no such function), SEM_ERR_ARGUMENTS or SEM_ERR_INTERNAL.
 */
sem_result_t sem_check_call(const funct_t *functions, const char *name, int arg_count, const symt_data_type_t *arg_types, symt_data_type_t *out_ret_type);

/**
   @brief Returns the declared type of a parameter of a built-in or user function.
   Intended for the code generator: an Int argument passed to a Double parameter has to be converted.
   @param functions Function table filled by the semantic analysis.
   @param name Function name.
   @param index Zero-based parameter index.
   @return Parameter type; SEM_TYPE_ANY for an Any parameter, an unknown function or a bad index.
 */
symt_data_type_t sem_param_type(const funct_t *functions, const char *name, int index);

#endif
