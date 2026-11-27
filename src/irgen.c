/**
 * @file irgen.c
 * @author xkralil00 (SusenkaOREOcze)
 * @date 2026-11-08
 * @brief Generates TAC from AST.
 */

// DISCLAIMER: The code was more readable before `make fix`.

#include "irgen.h"

#include <limits.h>
#include <stdlib.h>

typedef struct IrgenContext {
	tac_t *code;
	uint64_t temp;
	uint64_t label;
} irgen_context_t;

static irgen_result_t expression(irgen_context_t *ctx, const ast_node_t *node, tac_operand_t *out);
static irgen_result_t statement(irgen_context_t *ctx, const ast_node_t *node);

/**
 * @brief Appends one instruction and maps the result.
 * @param ctx Generation context.
 * @param ins Instruction data.
 * @return Generation status.
 */
static irgen_result_t emit(irgen_context_t *ctx, tac_instruction_t ins) {
	tac_result_t r = tac_append(ctx->code, &ins);
	return r == TAC_OK ? IRGEN_OK : r == TAC_MEMORY_ERROR ? IRGEN_MEMORY_ERROR : IRGEN_INVALID_AST;
}

/**
 * @brief Issues the next unique temporary operand.
 * @param ctx Generation context.
 * @param type Operand type.
 * @param[out] out New temporary.
 * @return Generation status.
 */
static irgen_result_t new_temp(irgen_context_t *ctx, symt_data_type_t type, tac_operand_t *out) {
	if (ctx->temp == UINT64_MAX)
		return IRGEN_INVALID_AST;

	*out = (tac_operand_t){.kind = TAC_OPERAND_TEMP, .type = type, .value.id = ctx->temp++};
	return IRGEN_OK;
}

/**
 * @brief Issues the next unique label number.
 * @param ctx Generation context.
 * @param[out] out New label.
 * @return Generation status.
 */
static irgen_result_t new_label(irgen_context_t *ctx, uint64_t *out) {
	if (ctx->label == UINT64_MAX)
		return IRGEN_INVALID_AST;

	*out = ctx->label++;
	return IRGEN_OK;
}

/**
 * @brief Converts an AST symbol into a TAC operand.
 * @param node Resolved node.
 * @param[out] out Symbol operand.
 * @return Generation status.
 */
static irgen_result_t symbol(const ast_node_t *node, tac_operand_t *out) {
	if (node == NULL || node->symbol_id == AST_UNRESOLVED_SYMBOL)
		return IRGEN_INVALID_AST;

	*out = (tac_operand_t){.kind = TAC_OPERAND_SYMBOL, .type = node->data_type, .value.id = node->symbol_id};
	return IRGEN_OK;
}

/**
 * @brief Captures a symbol value before later expressions can modify it.
 * @param ctx Generation context.
 * @param[in,out] operand Operand to freeze.
 * @return Generation status.
 */
static irgen_result_t freeze_symbol(irgen_context_t *ctx, tac_operand_t *operand) {
	if (operand->kind != TAC_OPERAND_SYMBOL)
		return IRGEN_OK;

	tac_operand_t saved;
	irgen_result_t r = new_temp(ctx, operand->type, &saved);
	if (r != IRGEN_OK)
		return r;

	r = emit(ctx, (tac_instruction_t){.opcode = TAC_MOVE, .dst = saved, .src1 = *operand});
	if (r == IRGEN_OK)
		*operand = saved;
	return r;

	/* example
	    Source: x + change_x()
	    Save x before the call, because change_x may modify x.

	    t1 = MOVE s7
	    t2 = CALL change_x, 0
	    t3 = ADD t1, t2
	*/
}

/**
 * @brief Emits a label or a branch to one label.
 * @param ctx Generation context.
 * @param op Branch opcode.
 * @param target Label number.
 * @param condition Conditional jump input, if needed.
 * @return Generation status.
 */
static irgen_result_t jump(irgen_context_t *ctx, tac_opcode_t op, uint64_t target, tac_operand_t condition) {
	return emit(ctx, (tac_instruction_t){.opcode = op, .label = target, .src1 = condition});

	/* example
	    Source: if condition then ...
	    Jump to the else branch when the condition is false.

	    JUMP_IF_FALSE t1, L2
	*/
}

/**
 * @brief Maps an AST operator to a TAC operator.
 * @param kind AST node kind.
 * @param[out] op Corresponding opcode.
 * @return Generation status.
 */
static irgen_result_t operator_code(ast_node_kind_t kind, tac_opcode_t *op) {
	switch (kind) {
	case AST_NODE_POSITIVE:
		*op = TAC_POSITIVE;
		break;
	case AST_NODE_NEGATIVE:
		*op = TAC_NEGATIVE;
		break;
	case AST_NODE_NOT:
		*op = TAC_NOT;
		break;
	case AST_NODE_ADD:
		*op = TAC_ADD;
		break;
	case AST_NODE_SUBTRACT:
		*op = TAC_SUBTRACT;
		break;
	case AST_NODE_MULTIPLY:
		*op = TAC_MULTIPLY;
		break;
	case AST_NODE_DIVIDE:
		*op = TAC_DIVIDE;
		break;
	case AST_NODE_CONCATENATE:
		*op = TAC_CONCATENATE;
		break;
	case AST_NODE_LESS:
		*op = TAC_LESS;
		break;
	case AST_NODE_LESS_EQUAL:
		*op = TAC_LESS_EQUAL;
		break;
	case AST_NODE_GREATER:
		*op = TAC_GREATER;
		break;
	case AST_NODE_GREATER_EQUAL:
		*op = TAC_GREATER_EQUAL;
		break;
	case AST_NODE_EQUAL:
		*op = TAC_EQUAL;
		break;
	case AST_NODE_NOT_EQUAL:
		*op = TAC_NOT_EQUAL;
		break;
	default:
		return IRGEN_INVALID_AST;
	}
	return IRGEN_OK;
}
// This could technically be removed if AST operators were to be used.

/**
 * @brief Evaluates call arguments before emitting ARG instructions.
 * @param ctx Generation context.
 * @param node Call node.
 * @param[out] out Result operand.
 * @return Generation status.
 */
static irgen_result_t call(irgen_context_t *ctx, const ast_node_t *node, tac_operand_t *out) {
	if (node->text == NULL)
		return IRGEN_INVALID_AST;

	size_t count = ast_node_child_count(node);
	if (count > INT64_MAX || count > SIZE_MAX / sizeof(tac_operand_t))
		return IRGEN_INVALID_AST;

	tac_operand_t *args = count ? calloc(count, sizeof(*args)) : NULL;
	if (count && args == NULL)
		return IRGEN_MEMORY_ERROR;

	irgen_result_t r = IRGEN_OK;
	size_t i = 0;
	for (const ast_node_t *arg = node->first_child; arg; arg = arg->next_sibling, ++i) {
		r = expression(ctx, arg, &args[i]);
		if (r != IRGEN_OK)
			goto cleanup;
		// A later argument may mutate this symbol through a call.
		if ((r = freeze_symbol(ctx, &args[i])) != IRGEN_OK)
			goto cleanup;
	}

	// Pass the evaluated values to the call in source order.
	for (i = 0; i < count; ++i)
		if ((r = emit(ctx, (tac_instruction_t){.opcode = TAC_ARG, .src1 = args[i]})) != IRGEN_OK)
			goto cleanup;

	// Unit calls have no result destination.
	if (node->data_type == SYMT_TYPE_UNIT)
		*out = (tac_operand_t){.kind = TAC_OPERAND_UNIT, .type = SYMT_TYPE_UNIT};

	else if ((r = new_temp(ctx, node->data_type, out)) != IRGEN_OK)
		goto cleanup;

	r = emit(ctx, (tac_instruction_t){.opcode = TAC_CALL,
	                                  .dst = node->data_type == SYMT_TYPE_UNIT ? (tac_operand_t){0} : *out,
	                                  .src1 = {.kind = TAC_OPERAND_INTEGER,
	                                           .type = SYMT_TYPE_INTEGER,
	                                           .value.integer = (int64_t)count},
	                                  .name = node->text});
cleanup:
	free(args);
	return r;

	/* example
	    Source: outer(value, inner(4))
	    Evaluate nested calls first, then pass outer's arguments.

	    t1 = MOVE s7
	    ARG 4
	    t2 = CALL inner, 1
	    ARG t1
	    ARG t2
	    t3 = CALL outer, 2
	*/
}

/**
 * @brief Emits lazy AND or OR evaluation.
 * @param ctx Generation context.
 * @param node Boolean operator node.
 * @param[out] out Boolean result temporary.
 * @return Generation status.
 */
static irgen_result_t logical(irgen_context_t *ctx, const ast_node_t *node, tac_operand_t *out) {
	const ast_node_t *left = node->first_child;
	const ast_node_t *right = left ? left->next_sibling : NULL;

	if (!right || right->next_sibling)
		return IRGEN_INVALID_AST;

	tac_operand_t first, second;
	uint64_t other, end;
	irgen_result_t r;
	if ((r = expression(ctx, left, &first)) != IRGEN_OK)
		return r;
	if ((r = new_temp(ctx, SYMT_TYPE_BOOLEAN, out)) != IRGEN_OK)
		return r;
	if ((r = new_label(ctx, &other)) != IRGEN_OK)
		return r;
	if ((r = new_label(ctx, &end)) != IRGEN_OK)
		return r;
	if ((r = jump(ctx, TAC_JUMP_IF_FALSE, other, first)) != IRGEN_OK)
		return r;
	if (node->kind == AST_NODE_OR) {
		if ((r = emit(ctx, (tac_instruction_t){.opcode = TAC_MOVE,
		                                       .dst = *out,
		                                       .src1 = {.kind = TAC_OPERAND_BOOLEAN,
		                                                .type = SYMT_TYPE_BOOLEAN,
		                                                .value.boolean = true}})) != IRGEN_OK)
			return r;
		if ((r = jump(ctx, TAC_JUMP, end, (tac_operand_t){0})) != IRGEN_OK)
			return r;
		if ((r = jump(ctx, TAC_LABEL, other, (tac_operand_t){0})) != IRGEN_OK)
			return r;
		if ((r = expression(ctx, right, &second)) != IRGEN_OK)
			return r;
		if ((r = emit(ctx, (tac_instruction_t){.opcode = TAC_MOVE, .dst = *out, .src1 = second})) != IRGEN_OK)
			return r;

	} else {
		if ((r = expression(ctx, right, &second)) != IRGEN_OK)
			return r;
		if ((r = emit(ctx, (tac_instruction_t){.opcode = TAC_MOVE, .dst = *out, .src1 = second})) != IRGEN_OK)
			return r;
		if ((r = jump(ctx, TAC_JUMP, end, (tac_operand_t){0})) != IRGEN_OK)
			return r;
		if ((r = jump(ctx, TAC_LABEL, other, (tac_operand_t){0})) != IRGEN_OK)
			return r;
		if ((r = emit(ctx, (tac_instruction_t){.opcode = TAC_MOVE,
		                                       .dst = *out,
		                                       .src1 = {.kind = TAC_OPERAND_BOOLEAN,
		                                                .type = SYMT_TYPE_BOOLEAN,
		                                                .value.boolean = false}})) != IRGEN_OK)
			return r;
	}
	return jump(ctx, TAC_LABEL, end, (tac_operand_t){0});

	/* example
	    Source: ready && check()
	    A false left side skips check() and gives false as the result.

	    JUMP_IF_FALSE s1, Lfalse
	    t2 = CALL check, 0
	    t1 = MOVE t2
	    JUMP Lend
	    Lfalse:
	        t1 = MOVE false
	    Lend:
	*/
}

/**
 * @brief Evaluates a block whose final child returns a value or exits with return.
 * @param ctx Generation context.
 * @param node Branch body.
 * @param dst Shared result temporary, or NONE when the result is Unit.
 * @param[out] returns True when the block ends with a return statement.
 * @return Generation status.
 */
static irgen_result_t value_block(irgen_context_t *ctx, const ast_node_t *node, tac_operand_t dst,
								  bool *returns) {
	if (!node || node->kind != AST_NODE_BLOCK || !node->first_child)
		return IRGEN_INVALID_AST;

	const ast_node_t *last = node->first_child;
	while (last->next_sibling)
		last = last->next_sibling;
	if (!ast_node_is_expression(last) && last->kind != AST_NODE_IF && last->kind != AST_NODE_RETURN)
		return IRGEN_INVALID_AST;
	for (const ast_node_t *item = node->first_child; item != last; item = item->next_sibling) {
		irgen_result_t r = statement(ctx, item);
		if (r != IRGEN_OK)
			return r;
	}
	if (last->kind == AST_NODE_RETURN) {
		*returns = true;
		return statement(ctx, last);
	}

	*returns = false;
	tac_operand_t value;
	irgen_result_t r = expression(ctx, last, &value);
	if (r != IRGEN_OK)
		return r;
	if (dst.kind == TAC_OPERAND_NONE)
		return value.kind == TAC_OPERAND_UNIT ? IRGEN_OK : IRGEN_INVALID_AST;
	return emit(ctx, (tac_instruction_t){.opcode = TAC_MOVE, .dst = dst, .src1 = value});

	/* example
	    Source block: { prepare(); x + 1 }
	    The last expression becomes the block's value.

	    CALL prepare, 0
	    t1 = MOVE s7
	    t2 = ADD t1, 1
	    t3 = MOVE t2
	*/
}

/**
 * @brief Emits a value-producing if with both branches joining in one temporary.
 * @param ctx Generation context.
 * @param node IF node with condition and two value-producing blocks.
 * @param[out] out Joined result operand.
 * @return Generation status.
 */
static irgen_result_t conditional_value(irgen_context_t *ctx, const ast_node_t *node, tac_operand_t *out) {
	const ast_node_t *condition = node->first_child;
	const ast_node_t *yes = condition ? condition->next_sibling : NULL;
	const ast_node_t *no = yes ? yes->next_sibling : NULL;

	if (!no || no->next_sibling)
		return IRGEN_INVALID_AST;

	tac_operand_t test;
	uint64_t alternative, end = 0;
	irgen_result_t r;
	if ((r = expression(ctx, condition, &test)) != IRGEN_OK)
		return r;
	const bool unit_result = node->data_type == SYMT_TYPE_UNIT;
	if (unit_result)
		*out = (tac_operand_t){.kind = TAC_OPERAND_UNIT, .type = SYMT_TYPE_UNIT};
	else if ((r = new_temp(ctx, node->data_type, out)) != IRGEN_OK)
		return r;
	if ((r = new_label(ctx, &alternative)) != IRGEN_OK)
		return r;
	if ((r = jump(ctx, TAC_JUMP_IF_FALSE, alternative, test)) != IRGEN_OK)
		return r;
	bool yes_returns;
	if ((r = value_block(ctx, yes, unit_result ? (tac_operand_t){0} : *out, &yes_returns)) != IRGEN_OK)
		return r;
	if (yes_returns) {
		if ((r = jump(ctx, TAC_LABEL, alternative, (tac_operand_t){0})) != IRGEN_OK)
			return r;
	} else {
		if ((r = new_label(ctx, &end)) != IRGEN_OK)
			return r;
		if ((r = jump(ctx, TAC_JUMP, end, (tac_operand_t){0})) != IRGEN_OK)
			return r;
		if ((r = jump(ctx, TAC_LABEL, alternative, (tac_operand_t){0})) != IRGEN_OK)
			return r;
	}
	bool no_returns;
	if ((r = value_block(ctx, no, unit_result ? (tac_operand_t){0} : *out, &no_returns)) != IRGEN_OK)
		return r;
	(void)no_returns;
	return yes_returns ? IRGEN_OK : jump(ctx, TAC_LABEL, end, (tac_operand_t){0});

	/* example
	    Source: if condition then { 10 } else { 20 }
	    Either branch writes the value consumed after the join.

	    JUMP_IF_FALSE condition, Lelse
	    t1 = MOVE 10
	    JUMP Lend
	    Lelse:
	    t1 = MOVE 20
	    Lend:
	*/
}

/**
 * @brief Emits one expression.
 * @param ctx Generation context.
 * @param node Expression node.
 * @param[out] out Result operand.
 * @return Generation status.
 */
static irgen_result_t expression(irgen_context_t *ctx, const ast_node_t *node, tac_operand_t *out) {
	if (!node || !out)
		return IRGEN_INVALID_AST;

	if (!node->first_child) {
		// Leaf expressions can be returned directly as TAC operands.
		switch (node->kind) {
		case AST_NODE_IDENTIFIER:
			return symbol(node, out);

		case AST_NODE_INTEGER:
			*out = (tac_operand_t){
			    .kind = TAC_OPERAND_INTEGER, .type = SYMT_TYPE_INTEGER, .value.integer = node->value.integer};
			return IRGEN_OK;

		case AST_NODE_DOUBLE:
			*out = (tac_operand_t){
			    .kind = TAC_OPERAND_DOUBLE, .type = SYMT_TYPE_DOUBLE, .value.decimal = node->value.decimal};
			return IRGEN_OK;

		case AST_NODE_STRING:
			if (!node->text)
				return IRGEN_INVALID_AST;

			*out = (tac_operand_t){
			    .kind = TAC_OPERAND_STRING, .type = SYMT_TYPE_STRING, .value.string = node->text};
			return IRGEN_OK;

		case AST_NODE_BOOLEAN:
			*out = (tac_operand_t){
			    .kind = TAC_OPERAND_BOOLEAN, .type = SYMT_TYPE_BOOLEAN, .value.boolean = node->value.boolean};
			return IRGEN_OK;

		case AST_NODE_UNIT:
			*out = (tac_operand_t){.kind = TAC_OPERAND_UNIT, .type = SYMT_TYPE_UNIT};
			return IRGEN_OK;

		default:
			break;
		}
	}

	if (node->kind == AST_NODE_CALL)
		return call(ctx, node, out);

	if (node->kind == AST_NODE_IF)
		return conditional_value(ctx, node, out);

	if (node->kind == AST_NODE_AND || node->kind == AST_NODE_OR)
		return logical(ctx, node, out);

	// Operators use one operand for unary nodes and two for binary nodes.
	const ast_node_t *left = node->first_child;
	const ast_node_t *right = left ? left->next_sibling : NULL;
	tac_opcode_t op;
	if (!left || operator_code(node->kind, &op) != IRGEN_OK)
		return IRGEN_INVALID_AST;

	if (ast_node_is_unary_operator(node) ? right != NULL : (!right || right->next_sibling))
		return IRGEN_INVALID_AST;

	tac_operand_t first, second = {0};
	irgen_result_t r;
	if ((r = expression(ctx, left, &first)) != IRGEN_OK)
		return r;
	if (right && (r = freeze_symbol(ctx, &first)) != IRGEN_OK)
		return r;
	if (right && (r = expression(ctx, right, &second)) != IRGEN_OK)
		return r;
	if ((r = new_temp(ctx, node->data_type, out)) != IRGEN_OK)
		return r;

	return emit(ctx, (tac_instruction_t){.opcode = op, .dst = *out, .src1 = first, .src2 = second});

	/* example
	    Source: -(x + 2)
	    Generate the inner expression first, then apply unary NEGATIVE.

	    t1 = MOVE s7
	    t2 = ADD t1, 2
	    t3 = NEGATIVE t2
	*/
}

/**
 * @brief Emits all statements of a block in source order.
 * @param ctx Generation context.
 * @param node BLOCK node.
 * @return Generation status.
 */
static irgen_result_t block(irgen_context_t *ctx, const ast_node_t *node) {
	if (!node || node->kind != AST_NODE_BLOCK)
		return IRGEN_INVALID_AST;

	// A block's children already follow source order.
	for (const ast_node_t *child = node->first_child; child; child = child->next_sibling) {
		irgen_result_t r = statement(ctx, child);
		if (r != IRGEN_OK)
			return r;
	}
	return IRGEN_OK;

	/* example
	    Source block: { val x = 1; print(x) }
	    Emit each child statement in order.

	    s7 = MOVE 1
	    ARG s7
	    CALL print, 1
	*/
}

/**
 * @brief Emits a conditional statement and its optional else branch.
 * @param ctx Generation context.
 * @param node IF node ordered as condition, then block, else block.
 * @return Generation status.
 */
static irgen_result_t conditional(irgen_context_t *ctx, const ast_node_t *node) {
	const ast_node_t *cond = node->first_child;
	const ast_node_t *yes = cond ? cond->next_sibling : NULL;
	const ast_node_t *no = yes ? yes->next_sibling : NULL;

	if (!yes || yes->kind != AST_NODE_BLOCK || (no && (no->kind != AST_NODE_BLOCK || no->next_sibling)))
		return IRGEN_INVALID_AST;

	tac_operand_t value;
	uint64_t other, end;
	irgen_result_t r;
	if ((r = expression(ctx, cond, &value)) != IRGEN_OK)
		return r;
	if ((r = new_label(ctx, &other)) != IRGEN_OK)
		return r;
	if ((r = jump(ctx, TAC_JUMP_IF_FALSE, other, value)) != IRGEN_OK)
		return r;
	if ((r = block(ctx, yes)) != IRGEN_OK)
		return r;
	// Without an else branch, the false path continues at this label.
	if (!no)
		return jump(ctx, TAC_LABEL, other, (tac_operand_t){0});
	if ((r = new_label(ctx, &end)) != IRGEN_OK)
		return r;
	if ((r = jump(ctx, TAC_JUMP, end, (tac_operand_t){0})) != IRGEN_OK)
		return r;
	if ((r = jump(ctx, TAC_LABEL, other, (tac_operand_t){0})) != IRGEN_OK)
		return r;
	if ((r = block(ctx, no)) != IRGEN_OK)
		return r;
	return jump(ctx, TAC_LABEL, end, (tac_operand_t){0});

	/* example
	    Source: if ready then { print("yes") } else { print("no") }
	    The first jump selects the else path; the second skips it after then.

	    JUMP_IF_FALSE ready, Lelse
	    ARG "yes"
	    CALL print, 1
	    JUMP Lend
	    Lelse:
	    ARG "no"
	    CALL print, 1
	    Lend:
	*/
}

/**
 * @brief Emits a condition-tested while loop.
 * @param ctx Generation context.
 * @param node WHILE node ordered as condition and body.
 * @return Generation status.
 */
static irgen_result_t loop(irgen_context_t *ctx, const ast_node_t *node) {
	const ast_node_t *cond = node->first_child;
	const ast_node_t *body = cond ? cond->next_sibling : NULL;

	if (!body || body->kind != AST_NODE_BLOCK || body->next_sibling)
		return IRGEN_INVALID_AST;

	uint64_t start, end;
	tac_operand_t value;
	irgen_result_t r;
	if ((r = new_label(ctx, &start)) != IRGEN_OK)
		return r;
	if ((r = new_label(ctx, &end)) != IRGEN_OK)
		return r;
	if ((r = jump(ctx, TAC_LABEL, start, (tac_operand_t){0})) != IRGEN_OK)
		return r;
	// Re-evaluate the condition on every iteration.
	if ((r = expression(ctx, cond, &value)) != IRGEN_OK)
		return r;
	if ((r = jump(ctx, TAC_JUMP_IF_FALSE, end, value)) != IRGEN_OK)
		return r;
	if ((r = block(ctx, body)) != IRGEN_OK)
		return r;
	if ((r = jump(ctx, TAC_JUMP, start, (tac_operand_t){0})) != IRGEN_OK)
		return r;
	return jump(ctx, TAC_LABEL, end, (tac_operand_t){0});

	/* example
	    Source: while ready do { work() }
	    The condition is checked at Lstart before each body execution.

	    Lstart:
	    JUMP_IF_FALSE ready, Lend
	    CALL work, 0
	    JUMP Lstart
	    Lend:
	*/
}

/**
 * @brief Emits an integer range for loop with optional guards.
 * @param ctx Generation context.
 * @param node FOR node ordered as binding with its range child, guards, body.
 * @return Generation status.
 */
static irgen_result_t range_loop(irgen_context_t *ctx, const ast_node_t *node) {
	const ast_node_t *binding = node->first_child;
	const ast_node_t *range = binding ? binding->first_child : NULL;

	if (!binding || binding->kind != AST_NODE_FOR_BINDING || !range ||
	    (range->kind != AST_NODE_RANGE_TO && range->kind != AST_NODE_RANGE_UNTIL) ||
	    range->next_sibling)
		return IRGEN_INVALID_AST;

	const ast_node_t *from = range->first_child;
	const ast_node_t *to = from ? from->next_sibling : NULL;
	const ast_node_t *by = to ? to->next_sibling : NULL;

	if (!to || (by && (range->kind != AST_NODE_RANGE_TO || by->next_sibling)))
		return IRGEN_INVALID_AST;

	const ast_node_t *body = binding->next_sibling;
	if (!body)
		return IRGEN_INVALID_AST;

	while (body->next_sibling)
		body = body->next_sibling;
	if (body->kind != AST_NODE_BLOCK)
		return IRGEN_INVALID_AST;

	// Evaluate the range expressions once before entering the loop.
	tac_operand_t variable, first, bound, increment, limit, step, test, updated;
	irgen_result_t r;
	if ((r = symbol(binding, &variable)) != IRGEN_OK)
		return r;
	if ((r = expression(ctx, from, &first)) != IRGEN_OK)
		return r;
	if ((r = freeze_symbol(ctx, &first)) != IRGEN_OK)
		return r;
	if ((r = expression(ctx, to, &bound)) != IRGEN_OK)
		return r;
	if (by && (r = freeze_symbol(ctx, &bound)) != IRGEN_OK)
		return r;
	if (by && (r = expression(ctx, by, &increment)) != IRGEN_OK)
		return r;
	if (!by)
		increment =
		    (tac_operand_t){.kind = TAC_OPERAND_INTEGER, .type = SYMT_TYPE_INTEGER, .value.integer = 1};

	if (increment.kind == TAC_OPERAND_INTEGER && increment.value.integer == 0)
		return IRGEN_INVALID_AST;
	// Save the upper bound and step.
	if ((r = new_temp(ctx, SYMT_TYPE_INTEGER, &limit)) != IRGEN_OK)
		return r;
	if ((r = new_temp(ctx, SYMT_TYPE_INTEGER, &step)) != IRGEN_OK)
		return r;
	if ((r = emit(ctx, (tac_instruction_t){.opcode = TAC_MOVE, .dst = variable, .src1 = first})) != IRGEN_OK)
		return r;
	if ((r = emit(ctx, (tac_instruction_t){.opcode = TAC_MOVE, .dst = limit, .src1 = bound})) != IRGEN_OK)
		return r;
	if ((r = emit(ctx, (tac_instruction_t){.opcode = TAC_MOVE, .dst = step, .src1 = increment})) != IRGEN_OK)
		return r;
	if (increment.kind != TAC_OPERAND_INTEGER &&
	    (r = emit(ctx, (tac_instruction_t){.opcode = TAC_CHECK_NONZERO, .src1 = step})) != IRGEN_OK)
		return r;

	uint64_t start, advance, end;
	if ((r = new_label(ctx, &start)) != IRGEN_OK)
		return r;
	if ((r = new_label(ctx, &advance)) != IRGEN_OK)
		return r;
	if ((r = new_label(ctx, &end)) != IRGEN_OK)
		return r;
	if ((r = jump(ctx, TAC_LABEL, start, (tac_operand_t){0})) != IRGEN_OK)
		return r;
	// The range comparison is emitted at the loop head.
	if ((r = new_temp(ctx, SYMT_TYPE_BOOLEAN, &test)) != IRGEN_OK)
		return r;
	if (increment.kind == TAC_OPERAND_INTEGER) {
		tac_opcode_t cmp = increment.value.integer > 0
		                       ? (range->kind == AST_NODE_RANGE_TO ? TAC_LESS_EQUAL : TAC_LESS)
		                       : (range->kind == AST_NODE_RANGE_TO ? TAC_GREATER_EQUAL : TAC_GREATER);
		if ((r = emit(ctx, (tac_instruction_t){
		                       .opcode = cmp, .dst = test, .src1 = variable, .src2 = limit})) != IRGEN_OK)
			return r;
	} else {
		// Select ascending or descending comparison each iteration.
		tac_operand_t positive;
		uint64_t negative, compared;
		if ((r = new_label(ctx, &negative)) != IRGEN_OK)
			return r;
		if ((r = new_label(ctx, &compared)) != IRGEN_OK)
			return r;
		if ((r = new_temp(ctx, SYMT_TYPE_BOOLEAN, &positive)) != IRGEN_OK)
			return r;
		if ((r = emit(ctx, (tac_instruction_t){.opcode = TAC_GREATER,
		                                       .dst = positive,
		                                       .src1 = step,
		                                       .src2 = {.kind = TAC_OPERAND_INTEGER,
		                                                .type = SYMT_TYPE_INTEGER,
		                                                .value.integer = 0}})) != IRGEN_OK)
			return r;
		if ((r = jump(ctx, TAC_JUMP_IF_FALSE, negative, positive)) != IRGEN_OK)
			return r;
		if ((r = emit(ctx, (tac_instruction_t){.opcode = range->kind == AST_NODE_RANGE_TO ? TAC_LESS_EQUAL
		                                                                                  : TAC_LESS,
		                                       .dst = test,
		                                       .src1 = variable,
		                                       .src2 = limit})) != IRGEN_OK)
			return r;
		if ((r = jump(ctx, TAC_JUMP, compared, (tac_operand_t){0})) != IRGEN_OK)
			return r;
		if ((r = jump(ctx, TAC_LABEL, negative, (tac_operand_t){0})) != IRGEN_OK)
			return r;
		if ((r = emit(ctx, (tac_instruction_t){.opcode = range->kind == AST_NODE_RANGE_TO ? TAC_GREATER_EQUAL
		                                                                                  : TAC_GREATER,
		                                       .dst = test,
		                                       .src1 = variable,
		                                       .src2 = limit})) != IRGEN_OK)
			return r;
		if ((r = jump(ctx, TAC_LABEL, compared, (tac_operand_t){0})) != IRGEN_OK)
			return r;
	}

	if ((r = jump(ctx, TAC_JUMP_IF_FALSE, end, test)) != IRGEN_OK)
		return r;
	// A failed guard skips the body but still advances the loop variable.
	for (const ast_node_t *guard = binding->next_sibling; guard != body; guard = guard->next_sibling) {
		if ((r = expression(ctx, guard, &test)) != IRGEN_OK)
			return r;
		if ((r = jump(ctx, TAC_JUMP_IF_FALSE, advance, test)) != IRGEN_OK)
			return r;
	}
	if ((r = block(ctx, body)) != IRGEN_OK)
		return r;
	// Advance and return to the range check.
	if ((r = jump(ctx, TAC_LABEL, advance, (tac_operand_t){0})) != IRGEN_OK)
		return r;
	if ((r = new_temp(ctx, SYMT_TYPE_INTEGER, &updated)) != IRGEN_OK)
		return r;
	if ((r = emit(ctx, (tac_instruction_t){
	                       .opcode = TAC_ADD, .dst = updated, .src1 = variable, .src2 = step})) != IRGEN_OK)
		return r;
	if ((r = emit(ctx, (tac_instruction_t){.opcode = TAC_MOVE, .dst = variable, .src1 = updated})) !=
	    IRGEN_OK)
		return r;
	if ((r = jump(ctx, TAC_JUMP, start, (tac_operand_t){0})) != IRGEN_OK)
		return r;
	return jump(ctx, TAC_LABEL, end, (tac_operand_t){0});

	/* example
	    Source: for i <- 1 until 4 do { use(i) }
	    Check the upper bound, execute the body, then increment.

	    s20 = MOVE 1
	    t1 = MOVE 4
	    t2 = MOVE 1
	    Lstart:
	    t3 = LESS s20, t1
	    JUMP_IF_FALSE t3, Lend
	    ARG s20
	    CALL use, 1
	    Ladvance:
	    t4 = ADD s20, t2
	    s20 = MOVE t4
	    JUMP Lstart
	    Lend:
	*/
}

/**
 * @brief Emits one AST statement or a discarded expression.
 * @param ctx Generation context.
 * @param node Statement node.
 * @return Generation status.
 */
static irgen_result_t statement(irgen_context_t *ctx, const ast_node_t *node) {
	if (!node)
		return IRGEN_INVALID_AST;

	tac_operand_t dst, value;
	irgen_result_t r;
	switch (node->kind) {
	case AST_NODE_BLOCK:
		return block(ctx, node);
	case AST_NODE_IF:
		return conditional(ctx, node);
	case AST_NODE_WHILE:
		return loop(ctx, node);
	case AST_NODE_FOR:
		return range_loop(ctx, node);
	case AST_NODE_VAL_DECLARATION:
	case AST_NODE_VAR_DECLARATION:
		// Initialize the declared symbol with its evaluated value.
		if (!node->first_child || node->first_child->next_sibling)
			return IRGEN_INVALID_AST;
		if ((r = symbol(node, &dst)) != IRGEN_OK)
			return r;
		if ((r = expression(ctx, node->first_child, &value)) != IRGEN_OK)
			return r;
		return emit(ctx, (tac_instruction_t){.opcode = TAC_MOVE, .dst = dst, .src1 = value});

	case AST_NODE_ASSIGNMENT: {
		// Resolve the target symbol, then evaluate and store the right side.
		const ast_node_t *target = node->first_child;
		const ast_node_t *rhs = target ? target->next_sibling : NULL;
		if (!target || target->kind != AST_NODE_IDENTIFIER || !rhs || rhs->next_sibling)
			return IRGEN_INVALID_AST;
		if ((r = symbol(target, &dst)) != IRGEN_OK)
			return r;
		if ((r = expression(ctx, rhs, &value)) != IRGEN_OK)
			return r;
		return emit(ctx, (tac_instruction_t){.opcode = TAC_MOVE, .dst = dst, .src1 = value});
	}

	case AST_NODE_RETURN:
		// A return without an expression returns Unit.
		if (node->first_child && node->first_child->next_sibling)
			return IRGEN_INVALID_AST;
		if (!node->first_child)
			value = (tac_operand_t){.kind = TAC_OPERAND_UNIT, .type = SYMT_TYPE_UNIT};
		else if ((r = expression(ctx, node->first_child, &value)) != IRGEN_OK)
			return r;
		return emit(ctx, (tac_instruction_t){.opcode = TAC_RETURN, .src1 = value});

	default:
		return ast_node_is_expression(node) ? expression(ctx, node, &value) : IRGEN_INVALID_AST;
	}

	/* example
	    Source: var count = 0; count = count + 1; return count
	    Declarations and assignments write to the symbol ID; return reads it.

	    s12 = MOVE 0
	    t1 = MOVE s12
	    t2 = ADD t1, 1
	    s12 = MOVE t2
	    RETURN s12
	*/
}

/**
 * @brief Emits one function and its parameter declarations.
 * @param ctx Generation context.
 * @param node FUNCTION or MAIN_FUNCTION node.
 * @return Generation status.
 */
static irgen_result_t function(irgen_context_t *ctx, const ast_node_t *node) {
	if (!node->text)
		return IRGEN_INVALID_AST;

	irgen_result_t r = emit(ctx, (tac_instruction_t){.opcode = TAC_FUNCTION_BEGIN,
	                                                .name = node->text,
	                                                .is_entrypoint = node->kind == AST_NODE_MAIN_FUNCTION});
	if (r != IRGEN_OK)
		return r;

	const ast_node_t *body = node->first_child;
	while (body && body->kind == AST_NODE_PARAMETER) {
		// Emit the function's parameters before its body.
		tac_operand_t operand;
		if ((r = symbol(body, &operand)) != IRGEN_OK)
			return r;
		if ((r = emit(ctx, (tac_instruction_t){.opcode = TAC_PARAM, .dst = operand})) != IRGEN_OK)
			return r;
		body = body->next_sibling;
	}

	if (!body || body->kind != AST_NODE_BLOCK || body->next_sibling)
		return IRGEN_INVALID_AST;

	// Keep the final expression separate because it may be the implicit return.
	const ast_node_t *last = body->first_child;
	while (last && last->next_sibling)
		last = last->next_sibling;
	for (const ast_node_t *item = body->first_child; item && item != last; item = item->next_sibling)
		if ((r = statement(ctx, item)) != IRGEN_OK)
			return r;
	if (last && node->data_type != SYMT_TYPE_UNIT &&
	    (ast_node_is_expression(last) || last->kind == AST_NODE_IF)) {
		tac_operand_t value;
		if ((r = expression(ctx, last, &value)) != IRGEN_OK)
			return r;
		if ((r = emit(ctx, (tac_instruction_t){.opcode = TAC_RETURN, .src1 = value})) != IRGEN_OK)
			return r;
	} else if (last && (r = statement(ctx, last)) != IRGEN_OK)
		return r;
	if (node->data_type == SYMT_TYPE_UNIT && (!last || last->kind != AST_NODE_RETURN)) {
		if ((r = emit(ctx, (tac_instruction_t){
		                       .opcode = TAC_RETURN,
		                       .src1 = {.kind = TAC_OPERAND_UNIT, .type = SYMT_TYPE_UNIT}})) != IRGEN_OK)
			return r;
	}
	return emit(ctx, (tac_instruction_t){.opcode = TAC_FUNCTION_END, .name = node->text});

	/* example
	    Source: def sum(a: Int): Int = { val b = 2; a + b }
	    The last expression becomes the implicit return value.

	    FUNCTION_BEGIN sum
	    PARAM s10
	    s11 = MOVE 2
	    t1 = MOVE s10
	    t2 = ADD t1, s11
	    RETURN t2
	    FUNCTION_END sum
	*/
}

irgen_result_t irgen_generate(const ast_t *ast, tac_t **out) {
	if (!ast || !out)
		return IRGEN_INVALID_ARGUMENT;

	// Only a complete program tree can be converted.
	const ast_node_t *root = ast_root((ast_t *)ast);
	if (!root || root->kind != AST_NODE_PROGRAM)
		return IRGEN_INVALID_AST;

	irgen_context_t ctx = {.temp = 1, .label = 1};
	if (tac_create(&ctx.code) != TAC_OK)
		return IRGEN_MEMORY_ERROR;

	// Functions are emitted in the order in which they appear in the program.
	for (const ast_node_t *item = root->first_child; item; item = item->next_sibling) {
		irgen_result_t r = IRGEN_OK;
		if (item->kind == AST_NODE_FUNCTION || item->kind == AST_NODE_MAIN_FUNCTION)
			r = function(&ctx, item);

		else if (item->kind != AST_NODE_IMPORT)
			r = IRGEN_INVALID_AST;

		if (r != IRGEN_OK) {
			// Do not expose an incomplete TAC list to the caller.
			tac_destroy(&ctx.code);
			return r;
		}
	}

	// Return only with the completed TAC list.
	*out = ctx.code;
	return IRGEN_OK;
}
