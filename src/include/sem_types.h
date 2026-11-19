
/**
   @file sem_types.h
   @author xmejstj00 (kmejstr)
   @brief Type rules of IFJ26 used by the semantic analysis.

   The rules include the extensions BOOLTHEN (Boolean is a regular type with the operators !, && and ||), CONTROL and FUNEXP.
 */

#ifndef SEM_TYPES_H
#define SEM_TYPES_H

#include "ast.h"
#include "symtable_shared.h"

#include <stdbool.h>

/**
   @brief Result codes of the semantic analysis.

   The values are the compiler exit codes from section 6 of the IFJ26 specification, so a result can be passed straight to exit().
 */
typedef enum SemResult {
	SEM_OK = 0,
	SEM_ERR_UNDEFINED = 3, //!< Undefined function or variable; missing @main function.
	SEM_ERR_REDEFINED = 4, //!< Redefinition of a function or variable; more than one @main.
	SEM_ERR_ARGUMENTS = 5, //!< Wrong number or type of call arguments; parameters of @main.
	SEM_ERR_TYPE = 6,      //!< Type incompatibility or failed type inference.
	SEM_ERR_IMMUTABLE = 7, //!< Write to a val variable or to a function parameter.
	SEM_ERR_OTHER = 10,    //!< Other semantic errors (zero range step).
	SEM_ERR_INTERNAL = 99, //!< Internal compiler error (allocation failure, malformed AST).
} sem_result_t;

/**
   @brief Returns whether a type is Int or Double.
 */
bool sem_is_numeric(symt_data_type_t type);

/**
  @brief Returns whether the type is the type of a value (Unit, Int, Double, String or Boolean).

   Only SYMT_TYPE_UNDEFINED is not a value type.
 */
bool sem_is_value_type(symt_data_type_t type);

/**
   @brief Returns whether a value of type source can be used where type target is expected.

   The types must be equal, or source is Int and target is Double (the only implicit conversion).
 */
bool sem_assignable(symt_data_type_t target, symt_data_type_t source);

/**
   @brief Returns the common type of two branches of an if expression.

   @return The common type, or SYMT_TYPE_UNDEFINED when the types cannot be unified.
 */
symt_data_type_t sem_unify(symt_data_type_t first, symt_data_type_t second);

/**
   @brief Returns the result type of a binary operator.
   @param op Kind of the operator node (AST_NODE_ADD ... AST_NODE_OR).
   @param left Type of the left operand.
   @param right Type of the right operand.
   @return Result type, or SYMT_TYPE_UNDEFINED when the operands are not allowed (error 6).
 */
symt_data_type_t sem_binary_type(ast_node_kind_t op, symt_data_type_t left, symt_data_type_t right);

/**
   @brief Returns the result type of a unary operator.
   @param op Kind of the operator node (AST_NODE_POSITIVE, AST_NODE_NEGATIVE or AST_NODE_NOT).
   @param operand Type of the operand.
   @return Result type, or SYMT_TYPE_UNDEFINED when the operand is not allowed (error 6).
 */
symt_data_type_t sem_unary_type(ast_node_kind_t op, symt_data_type_t operand);

/**
   @brief Returns the IFJ26 name of a type for error messages.
 */
const char *sem_type_name(symt_data_type_t type);

#endif
