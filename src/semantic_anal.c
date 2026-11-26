/**
    @file semantic_anal.c
    @author xmejstj00 (Kmejstr)
    @brief Semantic analysis of IFJ26 programs.

    The analysis has three phases:

    1. Collection: every function definition is registered, so that functions can be called before
        their definition. Redefinitions and the rules for the @main function are checked here.
    2. Bodies: every function body is checked statement by statement. Each function uses its own
        symbol table with one scope per block.
    3. Final checks: functions without an explicit return type must not be recursive and the @main
        function must return Unit.

    The rules include the extensions FUNEXP (calls inside expressions), BOOLTHEN (Boolean values,
    if without else) and CONTROL (if as an expression, for loops).

    A function without an explicit return type gets its type from its body. When such a function is
    called before its body was checked, the body is checked on demand in the middle of the caller.
 */

#include "include/semantic_anal.h"

#include "include/functable.h"
#include "include/hashtable.h"
#include "include/symtable.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*======================
  Data types & constants
  ======================*/

typedef enum SemFuncProgres {
	SEM_FUNCTION_UNVISITED,
	SEM_FUNCTION_IN_PROGRESS,
	SEM_FUNCTION_DONE,
} sem_function_progress_t;

typedef struct SemFunction {
	ast_node_t *node;
	const char *name;              //!< Points into the AST node.
	symt_data_type_t declared_ret; //!< Return type from the header, SYMT_TYPE_UNDEFINED if omitted.
	symt_data_type_t ret;          //!< Final return type, SYMT_TYPE_UNDEFINED until it is known.
	sem_function_progress_t progression;

	size_t *callees; //!< For calling functions in functions
	size_t callee_count;
	size_t callee_capacity;
} sem_function_t;

DEFTYPE_HASHTABLE(size_t) sem_function_index_t;

/**
    What a statement or a block leaves behind, that is what matters.
 */
typedef struct SemFlow {
	symt_data_type_t type; //!< Type of the produced value (Unit for statements without a value).
	bool diverges;         //!< Every path ends with a return, so no value is produced.
} sem_flow_t;

/**
    State of one run of the analysis.
 */
typedef struct SemContext {
	funct_t *functable;

	sem_function_t *functions;
	size_t function_count;
	sem_function_index_t function_index;

	sem_function_t *current;
	symt_t *scope;
} sem_context_t;

/**
    Leaves the current function with the result of a failed check.
    Used as SEM_TRY(check) in functions returning sem_result_t.
 */
#define SEM_TRY(check)                                                                                       \
	do {                                                                                                     \
		const sem_result_t sem_try_result = (check);                                                         \
		if (sem_try_result != SEM_OK)                                                                        \
			return sem_try_result;                                                                           \
	} while (0)

static sem_result_t sem_analyze_expression(sem_context_t *ctx, ast_node_t *node, symt_data_type_t *out_type);
static sem_result_t sem_analyze_statement(sem_context_t *ctx, ast_node_t *node, bool value_used,
                                          sem_flow_t *out_flow);
static sem_result_t sem_analyze_function(sem_context_t *ctx, sem_function_t *function);

/**
 * @brief prints one semantic error as "<line>: semantic error: <message>." like the parser does
 *
 * @param node node giving the line, NULL when the error has no position
 * @param format printf format of the message, without the final dot
 */
static void sem_error(const ast_node_t *node, const char *format, ...) {
	if (node != NULL)
		fprintf(stderr, "%zu: ", node->location.line);
	fputs("semantic error: ", stderr);
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	fputs(".\n", stderr);
}

static sem_result_t sem_fail_malformed(const ast_node_t *node, const char *what) {
	fprintf(stderr, "%zu: internal error: malformed AST (%s).\n", node->location.line, what);
	return SEM_ERR_INTERNAL;
}
/**
     Returns last child
*/
static ast_node_t *sem_last_child(ast_node_t *node) {
	ast_node_t *child = node->first_child;
	while (child != NULL && child->next_sibling != NULL)
		child = child->next_sibling;
	return child;
}
/**
 * @brief returns the name of a node, the parser stores every name in node->text
 *
 * @param node declaration, assignment, call or function node
 * @return the name, NULL when it is missing or empty
 */
static const char *sem_node_name(ast_node_t *node) {
	if (node->text != NULL && node->text[0] != '\0')
		return node->text;
	return NULL;
}

/*========
    Scopes
==========*/

static sem_result_t sem_enter_scope(sem_context_t *ctx, const ast_node_t *node) {
	if (symt_inc_scope_depth(ctx->scope) != SYMTSTAT_OK) {
		fprintf(stderr, "%zu: internal error: cannot open a new scope.\n", node->location.line);
		return SEM_ERR_INTERNAL;
	}
	return SEM_OK;
}

static void sem_leave_scope(sem_context_t *ctx) { symt_dec_scope_depth(ctx->scope); }

/**
    Declares a variable in the current scope.

    @param node Node used as the position of a possible error.
    @param[out] out_uid Receives the unique identifier assigned to the variable.
 */

static sem_result_t sem_declare_variable(sem_context_t *ctx, const ast_node_t *node, const char *name,
                                         symt_data_type_t type, bool is_mutable, uint64_t *out_uid) {
	if (symt_exists_in_current_scope(ctx->scope, name)) {
		sem_error(node, "redefinition of %s in the same block", name);
		return SEM_ERR_REDEFINED;
	}

	symt_symbol_t symbol;
	symbol.data_type = type;
	symbol.mutable = is_mutable;
	symbol.uid = 0;
	if (symt_insert(ctx->scope, name, &symbol) != SYMTSTAT_OK) {
		fprintf(stderr, "internal error: cannot store symbol %s.\n", name);
		return SEM_ERR_INTERNAL;
	}

	*out_uid = symbol.uid;
	return SEM_OK;
}

/*=============
    Expressions
===============*/

/**
    Remembers that the current function calls another user function (for the recursion check).
*/

static sem_result_t sem_add_callee(sem_context_t *ctx, const ast_node_t *node, size_t callee_index) {
	sem_function_t *caller = ctx->current;

	for (size_t i = 0; i < caller->callee_count; i++) {
		if (caller->callees[i] == callee_index)
			return SEM_OK;
	}
	if (caller->callee_count == caller->callee_capacity) {
		size_t new_capacity;
		if (caller->callee_capacity == 0)
			new_capacity = 4;
		else
			new_capacity = 2 * caller->callee_capacity;
		size_t *new_callees = realloc(caller->callees, new_capacity * sizeof(*new_callees));
		if (new_callees == NULL) {
			fprintf(stderr, "%zu: internal error: out of memory.\n", node->location.line);
			return SEM_ERR_INTERNAL;
		}
		caller->callees = new_callees;
		caller->callee_capacity = new_capacity;
	}

	caller->callees[caller->callee_count++] = callee_index;
	return SEM_OK;
}

/**
    Makes sure the return type of a called user function is known.
*/
static sem_result_t sem_resolve_callee(sem_context_t *ctx, const ast_node_t *call, const char *name) {
	size_t index = 0;
	if (!HTBL_RETRIEVE(ctx->function_index, name, index)) {
		sem_error(call, "call of undefined function %s", name);
		return SEM_ERR_UNDEFINED;
	}

	sem_function_t *callee = &ctx->functions[index];
	SEM_TRY(sem_add_callee(ctx, call, index));

	if (callee->ret != SYMT_TYPE_UNDEFINED)
		return SEM_OK;

	/* The return type has to be inferred from the body of the callee */
	if (callee->progression == SEM_FUNCTION_IN_PROGRESS) {
		sem_error(call, "recursive function %s needs an explicit return type", name);
		return SEM_ERR_TYPE;
	}

	return sem_analyze_function(ctx, callee);
}

static sem_result_t sem_analyze_call(sem_context_t *ctx, ast_node_t *node, symt_data_type_t *out_type) {
	const char *name = node->text;
	if (name == NULL || name[0] == '\0')
		return sem_fail_malformed(node, "call without a function name");

	const size_t arg_count = ast_node_child_count(node);
	size_t alloc_count = arg_count;
	if (alloc_count == 0)
		alloc_count = 1;

	symt_data_type_t *arg_types = malloc(alloc_count * sizeof(*arg_types));
	if (arg_types == NULL) {
		fprintf(stderr, "internal error: out of memory.\n");
		return SEM_ERR_INTERNAL;
	}

	sem_result_t result = SEM_OK;

	/* Arguments are expressions (only terms without FUNEXP */
	size_t arg_index = 0;
	for (ast_node_t *arg = node->first_child; arg != NULL && result == SEM_OK; arg = arg->next_sibling)
		result = sem_analyze_expression(ctx, arg, &arg_types[arg_index++]);

	// value.member (VALMEM) is a call of a built-in whose name starts with a dot, the value is the argument
	const bool is_member = name[0] == '.';
	if (result == SEM_OK && is_member && sem_find_builtin(name) == NULL) {
		sem_error(node, "values have no member %s", name + 1);
		result = SEM_ERR_TYPE;
	}

	if (result == SEM_OK && sem_find_builtin(name) == NULL)
		result = sem_resolve_callee(ctx, node, name);

	if (result == SEM_OK) {
		symt_data_type_t ret_type = SYMT_TYPE_UNDEFINED;
		result = sem_check_call(ctx->functable, name, (int)arg_count, arg_types, &ret_type);

		if (result == SEM_ERR_ARGUMENTS && is_member && arg_index > 0) {
			// member used on a value of a wrong type is a type error, not a bad call
			sem_error(node, "member %s cannot be used on a value of type %s", name + 1,
			          sem_type_name(arg_types[0]));
			result = SEM_ERR_TYPE;
		} else if (result == SEM_ERR_ARGUMENTS)
			sem_error(node, "wrong number or types of arguments in call of %s", name);
		else if (result != SEM_OK)
			sem_error(node, "cannot check the call of %s", name);
		else
			*out_type = node->data_type = ret_type;
	}

	free(arg_types);
	return result;
}

/**
    Analyses the body of a branch or of a loop: a block, or the single statement of a one-line body.
    The body always forms its own scope.
 */

static sem_result_t sem_analyze_body(sem_context_t *ctx, ast_node_t *body, bool value_used,
                                     sem_flow_t *out_flow) {
	out_flow->type = SYMT_TYPE_UNIT;
	out_flow->diverges = false;

	SEM_TRY(sem_enter_scope(ctx, body));

	if (body->kind != AST_NODE_BLOCK) {
		SEM_TRY(sem_analyze_statement(ctx, body, value_used, out_flow));
	} else {
		/*The result of block is the result of its last staement*/
		for (ast_node_t *statement = body->first_child; statement != NULL;
		     statement = statement->next_sibling) {
			const bool is_last = statement->next_sibling == NULL;
			SEM_TRY(sem_analyze_statement(ctx, statement, value_used && is_last, out_flow));
		}
	}

	sem_leave_scope(ctx);
	return SEM_OK;
}

/**
   Analyses a condition of if, while or a for guard: it has to be of type Boolean.
*/
static sem_result_t sem_analyze_condition(sem_context_t *ctx, ast_node_t *condition) {
	symt_data_type_t type = SYMT_TYPE_UNDEFINED;
	SEM_TRY(sem_analyze_expression(ctx, condition, &type));

	if (type != SYMT_TYPE_BOOLEAN) {
		sem_error(condition, "condition has type %s, expected Boolean", sem_type_name(type));
		return SEM_ERR_TYPE;
	}
	return SEM_OK;
}

/**
   Analyses an if.

   With CONTROL an if is an expression: when its value is used (an if expression, or the last
   statement of a block whose value is used), both branches must produce a common type. A branch that
   ends with return produces no value and is skipped.

   @param value_used The value of the if is needed.
*/
static sem_result_t sem_analyze_if(sem_context_t *ctx, ast_node_t *node, bool value_used,
                                   sem_flow_t *out_flow) {
	ast_node_t *condition = ast_node_child_at(node, 0);
	ast_node_t *then_body = ast_node_child_at(node, 1);
	ast_node_t *else_body = ast_node_child_at(node, 2);

	if (condition == NULL || then_body == NULL)
		return sem_fail_malformed(node, "if without a condition or a body");

	SEM_TRY(sem_analyze_condition(ctx, condition));

	/* A missing else behaves like a branch with the value of ...*/
	sem_flow_t then_flow;
	sem_flow_t else_flow;
	else_flow.type = SYMT_TYPE_UNIT;
	else_flow.diverges = false;
	SEM_TRY(sem_analyze_body(ctx, then_body, value_used, &then_flow));
	if (else_body != NULL)
		SEM_TRY(sem_analyze_body(ctx, else_body, value_used, &else_flow));

	out_flow->diverges = then_flow.diverges && else_flow.diverges;
	out_flow->type = SYMT_TYPE_UNIT;
	if (!value_used || out_flow->diverges)
		return SEM_OK;

	if (then_flow.diverges)
		out_flow->type = else_flow.type;
	else if (else_flow.diverges)
		out_flow->type = then_flow.type;
	else
		out_flow->type = sem_unify(then_flow.type, else_flow.type);

	if (out_flow->type == SYMT_TYPE_UNDEFINED) {
		sem_error(node, "branches of if have incompatible types %s and %s", sem_type_name(then_flow.type),
		          sem_type_name(else_flow.type));
		return SEM_ERR_TYPE;
	}

	node->data_type = out_flow->type;
	return SEM_OK;
}

static sem_result_t sem_analyze_unary(sem_context_t *ctx, ast_node_t *node, symt_data_type_t *out_type) {
	ast_node_t *operand = ast_node_child_at(node, 0);
	if (operand == NULL)
		return sem_fail_malformed(node, "unary operator without an operand");

	symt_data_type_t operand_type = SYMT_TYPE_UNDEFINED;
	SEM_TRY(sem_analyze_expression(ctx, operand, &operand_type));

	const symt_data_type_t type = sem_unary_type(node->kind, operand_type);
	if (type == SYMT_TYPE_UNDEFINED) {
		sem_error(node, "unary operator cannot be applied to type %s", sem_type_name(operand_type));
		return SEM_ERR_TYPE;
	}

	node->data_type = type;
	*out_type = node->data_type;
	return SEM_OK;
}

/**
 * @brief checks a binary operator and its operands
 *
 * a long expression like a + b + c + ... is a chain of binary nodes down the left side, so the
 * chain is walked in a loop instead of recursion, otherwise a very long expression would overflow
 * the C stack, operands are still checked from left to right
 *
 * @param ctx analysis context
 * @param node binary operator node
 * @param[out] out_type gets the type of the whole expression
 * @return SEM_OK or the exit code of the first error
 */
static sem_result_t sem_analyze_binary(sem_context_t *ctx, ast_node_t *node, symt_data_type_t *out_type) {
	ast_node_t *leftmost = node;
	while (ast_node_is_binary_operator(leftmost)) {
		if (leftmost->first_child == NULL || leftmost->first_child->next_sibling == NULL)
			return sem_fail_malformed(leftmost, "binary operator without two operands");
		leftmost = leftmost->first_child;
	}

	symt_data_type_t left_type = SYMT_TYPE_UNDEFINED;
	SEM_TRY(sem_analyze_expression(ctx, leftmost, &left_type));

	// back up the chain, each operator joins the type so far with its right operand
	ast_node_t *current = leftmost;
	do {
		current = current->parent;
		ast_node_t *right = current->first_child->next_sibling;
		symt_data_type_t right_type = SYMT_TYPE_UNDEFINED;
		SEM_TRY(sem_analyze_expression(ctx, right, &right_type));

		const symt_data_type_t type = sem_binary_type(current->kind, left_type, right_type);
		if (type == SYMT_TYPE_UNDEFINED) {
			sem_error(current, "binary operator cannot be applied to types %s and %s",
			          sem_type_name(left_type), sem_type_name(right_type));
			return SEM_ERR_TYPE;
		}
		left_type = current->data_type = type;
	} while (current != node);

	*out_type = left_type;
	return SEM_OK;
}

static sem_result_t sem_analyze_identifier(sem_context_t *ctx, ast_node_t *node, symt_data_type_t *out_type) {
	if (node->text == NULL || node->text[0] == '\0')
		return sem_fail_malformed(node, "identifier without a name");

	symt_symbol_t symbol;
	if (symt_find_symbol(ctx->scope, node->text, &symbol) != SYMTSTAT_OK) {
		sem_error(node, "use of undefined variable %s", node->text);
		return SEM_ERR_UNDEFINED;
	}
	node->symbol_id = symbol.uid;
	*out_type = node->data_type = symbol.data_type;
	return SEM_OK;
}

/**
   Analyses an expression, stores its type into the node and returns.
*/

static sem_result_t sem_analyze_expression(sem_context_t *ctx, ast_node_t *node, symt_data_type_t *out_type) {
	switch (node->kind) {
	case AST_NODE_INTEGER:
		*out_type = node->data_type = SYMT_TYPE_INTEGER;
		return SEM_OK;
	case AST_NODE_DOUBLE:
		*out_type = node->data_type = SYMT_TYPE_DOUBLE;
		return SEM_OK;
	case AST_NODE_STRING:
		*out_type = node->data_type = SYMT_TYPE_STRING;
		return SEM_OK;
	case AST_NODE_BOOLEAN:
		*out_type = node->data_type = SYMT_TYPE_BOOLEAN;
		return SEM_OK;
	case AST_NODE_UNIT:
		*out_type = node->data_type = SYMT_TYPE_UNIT;
		return SEM_OK;

	case AST_NODE_IDENTIFIER:
		return sem_analyze_identifier(ctx, node, out_type);
	case AST_NODE_CALL:
		return sem_analyze_call(ctx, node, out_type);

	case AST_NODE_IF: {
		sem_flow_t flow;
		SEM_TRY(sem_analyze_if(ctx, node, true, &flow));
		if (ast_node_child_count(node) < 3) {
			// only for hand-built ASTs, the parser already rejects this with code 2 (section 5.3)
			sem_error(node, "if used as an expression must have an else branch");
			return SEM_ERR_TYPE;
		}

		if (flow.diverges) {
			sem_error(node, "if expression does not produce a value");
			return SEM_ERR_TYPE;
		}

		*out_type = flow.type;
		return SEM_OK;
	}

	default:
		if (ast_node_is_unary_operator(node))
			return sem_analyze_unary(ctx, node, out_type);
		if (ast_node_is_binary_operator(node))
			return sem_analyze_binary(ctx, node, out_type);
		return sem_fail_malformed(node, "expression expected");
	}
}

/*============
    Statements
==============*/

static sem_result_t sem_analyze_declaration(sem_context_t *ctx, ast_node_t *node) {
	const char *name = sem_node_name(node);
	ast_node_t *initializer = sem_last_child(node);
	if (name == NULL || initializer == NULL)
		return sem_fail_malformed(node, "declaration without a name or an initializer");

	/*The variable is visible only after its definiton, the initializer is checked first*/
	symt_data_type_t init_type = SYMT_TYPE_UNDEFINED;
	SEM_TRY(sem_analyze_expression(ctx, initializer, &init_type));

	symt_data_type_t type = node->data_type;
	if (type == SYMT_TYPE_UNDEFINED) {
		if (!sem_is_value_type(init_type)) {
			sem_error(node, "cannot infer the type of %s from a value of type %s", name,
			          sem_type_name(init_type));
			return SEM_ERR_TYPE;
		}
		type = init_type;
	} else if (!sem_assignable(type, init_type)) {
		sem_error(node, "cannot initialize %s of type %s with a value of type %s", name, sem_type_name(type),
		          sem_type_name(init_type));
		return SEM_ERR_TYPE;
	}

	uint64_t uid = AST_UNRESOLVED_SYMBOL;
	const bool is_mutable = node->kind == AST_NODE_VAR_DECLARATION;
	SEM_TRY(sem_declare_variable(ctx, node, name, type, is_mutable, &uid));

	node->data_type = type;
	node->symbol_id = uid;
	return SEM_OK;
}

static sem_result_t sem_analyze_assignment(sem_context_t *ctx, ast_node_t *node) {
	const char *name = sem_node_name(node);
	ast_node_t *value = sem_last_child(node);
	if (name == NULL || value == NULL)
		return sem_fail_malformed(node, "assignment without a name or a value");

	symt_symbol_t symbol;
	if (symt_find_symbol(ctx->scope, name, &symbol) != SYMTSTAT_OK) {
		sem_error(node, "assignment to undefined variable %s", name);
		return SEM_ERR_UNDEFINED;
	}
	if (!symbol.mutable) {
		sem_error(node, "cannot assign to immutable variable %s", name);
		return SEM_ERR_IMMUTABLE;
	}

	symt_data_type_t value_type = SYMT_TYPE_UNDEFINED;
	SEM_TRY(sem_analyze_expression(ctx, value, &value_type));
	if (!sem_assignable(symbol.data_type, value_type)) {
		sem_error(node, "cannot assign a value of type %s to %s of type %s", sem_type_name(value_type), name,
		          sem_type_name(symbol.data_type));
		return SEM_ERR_TYPE;
	}

	node->data_type = symbol.data_type;
	node->symbol_id = symbol.uid;
	return SEM_OK;
}

static sem_result_t sem_analyze_return(sem_context_t *ctx, ast_node_t *node) {
	const symt_data_type_t ret_type = ctx->current->declared_ret;
	ast_node_t *value = node->first_child;

	if (ret_type == SYMT_TYPE_UNDEFINED) {
		sem_error(node, "function %s uses return, so it needs an explicit return type", ctx->current->name);
		return SEM_ERR_TYPE;
	}

	if (value == NULL) {
		if (ret_type != SYMT_TYPE_UNIT) {
			sem_error(node, "return without a value in a function returning %s", sem_type_name(ret_type));
			return SEM_ERR_TYPE;
		}
	} else {
		symt_data_type_t value_type = SYMT_TYPE_UNDEFINED;
		SEM_TRY(sem_analyze_expression(ctx, value, &value_type));

		if (ret_type != SYMT_TYPE_UNIT && !sem_assignable(ret_type, value_type)) {
			sem_error(node, "cannot return a value of type %s from a function returning %s",
			          sem_type_name(value_type), sem_type_name(ret_type));
			return SEM_ERR_TYPE;
		}
	}
	node->data_type = ret_type;
	return SEM_OK;
}

static sem_result_t sem_analyze_while(sem_context_t *ctx, ast_node_t *node) {
	ast_node_t *condition = ast_node_child_at(node, 0);
	ast_node_t *body = ast_node_child_at(node, 1);
	if (condition == NULL || body == NULL)
		return sem_fail_malformed(node, "while without a condition or a body");

	SEM_TRY(sem_analyze_condition(ctx, condition));

	sem_flow_t body_flow;
	return sem_analyze_body(ctx, body, false, &body_flow);
}

static bool sem_is_zero_literal(const ast_node_t *node) {
	while ((node->kind == AST_NODE_NEGATIVE || node->kind == AST_NODE_POSITIVE) && node->first_child != NULL)
		node = node->first_child;
	return node->kind == AST_NODE_INTEGER && node->value.integer == 0;
}

static sem_result_t sem_analyze_range_part(sem_context_t *ctx, ast_node_t *part, const char *what) {
	symt_data_type_t type = SYMT_TYPE_UNDEFINED;
	SEM_TRY(sem_analyze_expression(ctx, part, &type));

	if (type != SYMT_TYPE_INTEGER) {
		sem_error(part, "%s of a for range has type %s, expected Int", what, sem_type_name(type));
		return SEM_ERR_TYPE;
	}
	return SEM_OK;
}

static sem_result_t sem_analyze_for(sem_context_t *ctx, ast_node_t *node) {
	ast_node_t *binding = node->first_child;
	ast_node_t *body = sem_last_child(node);
	if (binding == NULL || binding->kind != AST_NODE_FOR_BINDING || body == binding)
		return sem_fail_malformed(node, "for without a generator or a body");

	ast_node_t *range = binding->first_child;
	if (binding->text == NULL || binding->text[0] == '\0' || range == NULL ||
	    (range->kind != AST_NODE_RANGE_TO && range->kind != AST_NODE_RANGE_UNTIL))
		return sem_fail_malformed(range, "range without both bounds");

	ast_node_t *from = ast_node_child_at(range, 0);
	ast_node_t *to = ast_node_child_at(range, 1);
	ast_node_t *step = ast_node_child_at(range, 2);

	SEM_TRY(sem_analyze_range_part(ctx, from, "lower bound"));
	SEM_TRY(sem_analyze_range_part(ctx, to, "upper bound"));
	if (step != NULL) {
		SEM_TRY(sem_analyze_range_part(ctx, step, "step"));
		if (sem_is_zero_literal(step)) {
			sem_error(step, "step of a for range must not be zero");
			return SEM_ERR_OTHER;
		}
	}
	range->data_type = SYMT_TYPE_INTEGER;

	SEM_TRY(sem_enter_scope(ctx, node));

	uint64_t uid = AST_UNRESOLVED_SYMBOL;
	SEM_TRY(sem_declare_variable(ctx, binding, binding->text, SYMT_TYPE_INTEGER, false, &uid));
	binding->data_type = SYMT_TYPE_INTEGER;
	binding->symbol_id = uid;

	for (ast_node_t *guard = binding->next_sibling; guard != body; guard = guard->next_sibling)
		SEM_TRY(sem_analyze_condition(ctx, guard));

	sem_flow_t body_flow;
	SEM_TRY(sem_analyze_body(ctx, body, false, &body_flow));

	sem_leave_scope(ctx);
	return SEM_OK;
}

/**
    Analyses one statement.

    @param value_used The value of the statement is the result of the enclosing function body or of
    an if expression, so it has to be well-typed.
    @param[out] out_flow Receives what the statement produces.
 */
static sem_result_t sem_analyze_statement(sem_context_t *ctx, ast_node_t *node, bool value_used,
                                          sem_flow_t *out_flow) {
	out_flow->type = SYMT_TYPE_UNIT;
	out_flow->diverges = false;

	switch (node->kind) {
	case AST_NODE_VAL_DECLARATION:
	case AST_NODE_VAR_DECLARATION:
		return sem_analyze_declaration(ctx, node);
	case AST_NODE_ASSIGNMENT:
		return sem_analyze_assignment(ctx, node);
	case AST_NODE_WHILE:
		return sem_analyze_while(ctx, node);
	case AST_NODE_FOR:
		return sem_analyze_for(ctx, node);

	case AST_NODE_RETURN:
		out_flow->diverges = true;
		return sem_analyze_return(ctx, node);

	case AST_NODE_IF:
		return sem_analyze_if(ctx, node, value_used, out_flow);
	case AST_NODE_BLOCK:
		return sem_analyze_body(ctx, node, value_used, out_flow);

	default:
		if (!ast_node_is_expression(node))
			return sem_fail_malformed(node, "statement expected");
		return sem_analyze_expression(ctx, node, &out_flow->type);
	}
}

/*==========
   Functions
=============*/

/**
    Stores the definition of a function with a known return type into the function table.
*/
static sem_result_t sem_register_function(sem_context_t *ctx, const sem_function_t *function) {
	bool ok = funct_insert_begin(ctx->functable, IS_FUNC_DEF) == SYMTSTAT_OK;

	for (const ast_node_t *param = function->node->first_child;
	     ok && param != NULL && param->kind == AST_NODE_PARAMETER; param = param->next_sibling)
		ok = funct_insert_add_param(ctx->functable, param->data_type) == SYMTSTAT_OK;

	ok = ok && funct_insert_end(ctx->functable, function->name, function->ret) == SYMTSTAT_OK;
	if (!ok) {
		fprintf(stderr, "internal error: cannot store function %s into the function table.\n",
		        function->name);
		return SEM_ERR_INTERNAL;
	}
	return SEM_OK;
}

static sem_result_t sem_analyze_function_body(sem_context_t *ctx, sem_function_t *function) {
	ast_node_t *node = function->node;

	SEM_TRY(sem_enter_scope(ctx, node));
	ast_node_t *child = node->first_child;
	for (; child != NULL && child->kind == AST_NODE_PARAMETER; child = child->next_sibling) {
		uint64_t uid = AST_UNRESOLVED_SYMBOL;
		SEM_TRY(sem_declare_variable(ctx, child, child->text, child->data_type, false, &uid));
		child->symbol_id = uid;
	}
	if (child == NULL)
		return sem_fail_malformed(node, "function without a body");

	/*In a function declared as Unit the result of the body is thrown away*/
	const bool value_used = function->declared_ret != SYMT_TYPE_UNIT;

	sem_flow_t flow;
	flow.type = SYMT_TYPE_UNIT;
	flow.diverges = false;

	SEM_TRY(sem_enter_scope(ctx, node));
	for (ast_node_t *statement = child; statement != NULL; statement = statement->next_sibling) {
		const bool is_last = statement->next_sibling == NULL;
		SEM_TRY(sem_analyze_statement(ctx, statement, value_used && is_last, &flow));
	}
	sem_leave_scope(ctx);
	sem_leave_scope(ctx);

	if (function->declared_ret == SYMT_TYPE_UNDEFINED) {
		/*The return type is type of body result(a body with a return was rejected already)*/
		if (flow.diverges || !sem_is_value_type(flow.type)) {
			sem_error(function->node, "cannot infer the return type of function %s", function->name);
			return SEM_ERR_TYPE;
		}
		function->ret = flow.type;
		SEM_TRY(sem_register_function(ctx, function));
	} else if ((function->declared_ret != SYMT_TYPE_UNIT) && !flow.diverges &&
	           !sem_assignable(function->declared_ret, flow.type)) {
		sem_error(function->node, "body of function %s results in %s, but the function returns %s",
		          function->name, sem_type_name(flow.type), sem_type_name(function->declared_ret));
		return SEM_ERR_TYPE;
	}

	node->data_type = function->ret;
	return SEM_OK;
}

/**
    Analyses one function with a fresh symbol table.

    The function may be called while another function is being analysed (see sem_resolve_callee()),
    therefore the state of the interrupted function is saved and restored.
 */

static sem_result_t sem_analyze_function(sem_context_t *ctx, sem_function_t *function) {
	symt_t *scope = NULL;
	if (symt_init(&scope) != SYMTSTAT_OK) {
		fprintf(stderr, "internal error: cannot create a symbol table.\n");
		return SEM_ERR_INTERNAL;
	}

	sem_function_t *const outer_function = ctx->current;
	symt_t *const outer_scope = ctx->scope;
	ctx->current = function;
	ctx->scope = scope;
	function->progression = SEM_FUNCTION_IN_PROGRESS;

	const sem_result_t result = sem_analyze_function_body(ctx, function);

	function->progression = SEM_FUNCTION_DONE;
	ctx->current = outer_function;
	ctx->scope = outer_scope;
	symt_free(scope);
	return result;
}

/*========
   Program
===========*/

/**
    Phase 1: registers one function definition.
 */
static sem_result_t sem_collect_function(sem_context_t *ctx, ast_node_t *node, size_t *main_count) {
	const char *name = node->text;
	if (name == NULL || name[0] == '\0')
		return sem_fail_malformed(node, "function without a name");

	if (sem_find_builtin(name) != NULL) {
		sem_error(node, "redefinition of built-in function %s", name);
		return SEM_ERR_REDEFINED;
	}
	if (HTBL_EXISTS(ctx->function_index, name)) {
		sem_error(node, "redefinition of function %s", name);
		return SEM_ERR_REDEFINED;
	}

	size_t index = ctx->function_count++;
	sem_function_t *function = &ctx->functions[index];
	function->node = node;
	function->name = name;
	function->declared_ret = node->data_type;
	function->ret = node->data_type;
	function->progression = SEM_FUNCTION_UNVISITED;

	if (HTBL_INSERT(ctx->function_index, name, &index) != HTBLSTAT_OK) {
		fprintf(stderr, "internal error: out of memory.\n");
		return SEM_ERR_INTERNAL;
	}

	size_t param_count = 0;
	for (const ast_node_t *param = node->first_child; param != NULL && param->kind == AST_NODE_PARAMETER;
	     param = param->next_sibling) {
		if (param->text == NULL || param->text[0] == '\0' || param->data_type == SYMT_TYPE_UNDEFINED)
			return sem_fail_malformed(param, "parameter without a name or type");
		param_count++;
	}

	if (node->kind == AST_NODE_MAIN_FUNCTION) {
		if (++(*main_count) > 1) {
			sem_error(node, "more than one @main function");
			return SEM_ERR_REDEFINED;
		}
		if (param_count > 0) {
			sem_error(node, "@main function %s must not have parameters", name);
			return SEM_ERR_ARGUMENTS;
		}
		if (function->declared_ret != SYMT_TYPE_UNDEFINED && function->declared_ret != SYMT_TYPE_UNIT) {
			sem_error(node, "@main function %s must return Unit", name);
			return SEM_ERR_TYPE;
		}
	}

	if (function->ret != SYMT_TYPE_UNDEFINED)
		SEM_TRY(sem_register_function(ctx, function));
	return SEM_OK;
}

/**
    Returns whether a function can reach itself through calls of user functions.

    @param visited Working array of function_count elements.
    @param stack Working array of function_count elements.
*/
static bool sem_is_recursive(const sem_context_t *ctx, size_t start, bool *visited, size_t *stack) {
	memset(visited, 0, ctx->function_count * sizeof(*visited));
	size_t stack_size = 0;
	stack[stack_size++] = start;

	while (stack_size > 0) {
		const sem_function_t *function = &ctx->functions[stack[--stack_size]];

		for (size_t i = 0; i < function->callee_count; i++) {
			const size_t callee = function->callees[i];
			if (callee == start)
				return true;
			if (!visited[callee]) {
				visited[callee] = true;
				stack[stack_size++] = callee;
			}
		}
	}
	return false;
}

/**
    Phase 3: a function whose return type was inferred must not be recursive, not even indirectly
    hrough functions with explicit return types.
*/
static sem_result_t sem_check_recursion(sem_context_t *ctx) {
	if (ctx->function_count == 0)
		return SEM_OK;

	bool *visited = malloc(ctx->function_count * sizeof(*visited));
	size_t *stack = malloc(ctx->function_count * sizeof(*stack));
	sem_result_t result = SEM_OK;

	if (visited == NULL || stack == NULL) {
		free(visited);
		free(stack);
		fprintf(stderr, "internal error: out of memory.\n");
		return SEM_ERR_INTERNAL;
	}

	for (size_t i = 0; i < ctx->function_count && result == SEM_OK; i++) {
		const sem_function_t *function = &ctx->functions[i];
		if (function->declared_ret == SYMT_TYPE_UNDEFINED && sem_is_recursive(ctx, i, visited, stack)) {
			free(visited);
			free(stack);
			sem_error(function->node, "recursive function %s needs an explicit return type", function->name);
			return SEM_ERR_TYPE;
		}
	}

	free(visited);
	free(stack);
	return result;
}

static void sem_context_free(sem_context_t *ctx) {
	if (ctx->function_index.table != NULL)
		HTBL_FREE(ctx->function_index);
	for (size_t i = 0; i < ctx->function_count; i++)
		free(ctx->functions[i].callees);
	free(ctx->functions);
}

static sem_result_t sem_analyze_program(sem_context_t *ctx, ast_node_t *root) {
	/* Phase 1: collect all function definitions */
	size_t main_count = 0;
	for (ast_node_t *child = root->first_child; child != NULL; child = child->next_sibling) {
		if (child->kind == AST_NODE_IMPORT)
			continue;
		if (child->kind != AST_NODE_FUNCTION && child->kind != AST_NODE_MAIN_FUNCTION)
			return sem_fail_malformed(child, "only function definitions are allowed at the top level");
		SEM_TRY(sem_collect_function(ctx, child, &main_count));
	}
	if (main_count == 0) {
		sem_error(NULL, "missing @main function");
		return SEM_ERR_UNDEFINED;
	}

	/* Phase 2: analyse the bodies (some may have been analysed on demand already) */
	for (size_t i = 0; i < ctx->function_count; i++)
		if (ctx->functions[i].progression == SEM_FUNCTION_UNVISITED)
			SEM_TRY(sem_analyze_function(ctx, &ctx->functions[i]));

	/* Phase 3: checks that need the whole program */
	for (size_t i = 0; i < ctx->function_count; i++) {
		const sem_function_t *function = &ctx->functions[i];
		if (function->node->kind == AST_NODE_MAIN_FUNCTION && function->ret != SYMT_TYPE_UNIT) {
			sem_error(function->node, "@main function %s must return Unit", function->name);
			return SEM_ERR_TYPE;
		}
	}
	return sem_check_recursion(ctx);
}

sem_result_t sem_analyze(ast_t *ast, funct_t *functions) {
	ast_node_t *root = ast_root(ast);
	if (functions == NULL || root == NULL || root->kind != AST_NODE_PROGRAM) {
		fprintf(stderr, "internal error: no program to analyse.\n");
		return SEM_ERR_INTERNAL;
	}

	sem_context_t ctx = {.functable = functions};

	/* The number of top-level nodes is an upper bound of the number of functions */
	size_t top_level_count = ast_node_child_count(root);
	size_t alloc_count = top_level_count;
	if (alloc_count == 0)
		alloc_count = 1;

	ctx.functions = calloc(alloc_count, sizeof(*ctx.functions));
	if (ctx.functions == NULL || HTBL_INIT(ctx.function_index, 2 * top_level_count + 16) != HTBLSTAT_OK) {
		fprintf(stderr, "internal error: out of memory.\n");
		sem_context_free(&ctx);
		return SEM_ERR_INTERNAL;
	}

	sem_result_t result = sem_analyze_program(&ctx, root);
	sem_context_free(&ctx);
	return result;
}