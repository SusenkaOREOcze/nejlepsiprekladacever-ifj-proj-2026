/**
	@file sem_types.c
	@author xmejstj00 (kmejstr)
	@brief Type rules of IFJ26 (sections 1.3 and 3 of the specification, extension BOOLTHEN).
 */

#include "include/sem_types.h"

bool sem_is_numeric(symt_data_type_t type) {
	return type == SYMT_TYPE_INTEGER || type == SYMT_TYPE_DOUBLE;
}

bool sem_is_value_type(symt_data_type_t type) {
	return type != SYMT_TYPE_UNDEFINED;
}

bool sem_assignable(symt_data_type_t target, symt_data_type_t source) {
	if (target == SYMT_TYPE_UNDEFINED || source == SYMT_TYPE_UNDEFINED)
		return false;
	if (target == source)
		return true;
	return target == SYMT_TYPE_DOUBLE && source == SYMT_TYPE_INTEGER;
}

symt_data_type_t sem_unify(symt_data_type_t first, symt_data_type_t second) {
	if (first == second)
		return first;
	if (sem_is_numeric(first) && sem_is_numeric(second))
		return SYMT_TYPE_DOUBLE;
	return SYMT_TYPE_UNDEFINED;
}

symt_data_type_t sem_binary_type(ast_node_kind_t op, symt_data_type_t left, symt_data_type_t right) {
	/* An operand with an unknown type means an error was already found in it */
	if (!sem_is_value_type(left) || !sem_is_value_type(right))
		return SYMT_TYPE_UNDEFINED;

	const bool both_numeric = sem_is_numeric(left) && sem_is_numeric(right);
	const bool both_string = left == SYMT_TYPE_STRING && right == SYMT_TYPE_STRING;
	const bool both_boolean = left == SYMT_TYPE_BOOLEAN && right == SYMT_TYPE_BOOLEAN;

	switch (op) {
	case AST_NODE_ADD:
		if (both_string) /* String concatenation, hate this word */
			return SYMT_TYPE_STRING;
		[[fallthrough]];
	case AST_NODE_SUBTRACT:
	case AST_NODE_MULTIPLY:
	case AST_NODE_DIVIDE:
		if (!both_numeric)
			return SYMT_TYPE_UNDEFINED;
		/* Int op Int stays Int, anything with a Double is Double */
		return sem_unify(left, right);

	case AST_NODE_LESS:
	case AST_NODE_LESS_EQUAL:
	case AST_NODE_GREATER:
	case AST_NODE_GREATER_EQUAL:
		return (both_numeric || both_string) ? SYMT_TYPE_BOOLEAN : SYMT_TYPE_UNDEFINED;

	case AST_NODE_EQUAL:
	case AST_NODE_NOT_EQUAL:
		return (both_numeric || left == right) ? SYMT_TYPE_BOOLEAN : SYMT_TYPE_UNDEFINED;

	case AST_NODE_AND:
	case AST_NODE_OR:
		return both_boolean ? SYMT_TYPE_BOOLEAN : SYMT_TYPE_UNDEFINED;

	case AST_NODE_CONCATENATE:
		/* The ++ operator concatenates lists (extension LISTS), which are not implemented */
		return SYMT_TYPE_UNDEFINED;

	default:
		return SYMT_TYPE_UNDEFINED;
	}
}

symt_data_type_t sem_unary_type(ast_node_kind_t op, symt_data_type_t operand) {
	switch (op) {
	case AST_NODE_POSITIVE:
	case AST_NODE_NEGATIVE:
		return sem_is_numeric(operand) ? operand : SYMT_TYPE_UNDEFINED;

	case AST_NODE_NOT:
		return operand == SYMT_TYPE_BOOLEAN ? SYMT_TYPE_BOOLEAN : SYMT_TYPE_UNDEFINED;

	default:
		return SYMT_TYPE_UNDEFINED;
	}
}

const char *sem_type_name(symt_data_type_t type) {
	switch (type) {
	case SYMT_TYPE_UNIT:
		return "Unit";
	case SYMT_TYPE_INTEGER:
		return "Int";
	case SYMT_TYPE_DOUBLE:
		return "Double";
	case SYMT_TYPE_STRING:
		return "String";
	case SYMT_TYPE_BOOLEAN:
		return "Boolean";
	default:
		return "<unknown>";
	}
}
