/**
  @file optimizer.c
  @author xbosakd00 (DasBean125)
  @date 2026-11-21
  @brief Three-address code optimizer module.

  Performs various optimizations on a three-address code (TAC) linked list.
*/

#include "include/codegen_shared.h"
#include "include/optimizer_impl.h"
#include <stdio.h>
#include <stdlib.h>

/*=================
  Static globalists (lol)
  =================*/

/**
   @brief A function which evaluates one of TAC_* operation expressions.

   @param 1 The destination of the evaluated expression.
   @param 2 For unary operators: the source operand. For binary operators: the left-hand side source operand.
   @param 3 For unary operators: unused. For binary operators: the right-hand side source operand.
   @returns true if the result written to dst should be considered as a valid literal, false otherwise.

   All operands must be literal types (i.e. constant).
*/
typedef bool (*expression_evaluator_t)(tac_operand_t *, const tac_operand_t *, const tac_operand_t *);

/**
   @brief Evaluates a TAC_POSITIVE instruction.
   See @ref expression_evaluator_t for parameter information.
*/
bool opt_eval_instr_positive(tac_operand_t *dst, const tac_operand_t *src, const tac_operand_t *inv) {
	(void)inv;
	if (dst == NULL)
		return false;
	if (src == NULL) {
		dst->kind = TAC_OPERAND_NONE;
		dst->type = SYMT_TYPE_UNDEFINED;
		return false;
	}
	if (!tac_is_literal_operand(src->kind))
		return false;

	/* Integer src */
	if (src->type == SYMT_TYPE_INTEGER) {
		dst->kind = TAC_OPERAND_INTEGER;
		dst->type = SYMT_TYPE_INTEGER;
		dst->value.integer = +src->value.integer;
		return true;
	}
	/* Double src */
	if (src->type == SYMT_TYPE_DOUBLE) {
		dst->kind = TAC_OPERAND_DOUBLE;
		dst->type = SYMT_TYPE_DOUBLE;
		dst->value.decimal = +src->value.decimal;
		return true;
	}

	return false;
}

bool opt_eval_nop(tac_operand_t *dst, const tac_operand_t *src1, const tac_operand_t *src2) {
	(void)dst;
	(void)src1;
	(void)src2;
	return false;
}

static expression_evaluator_t const EXPRESSION_EVALS[] = {
    opt_eval_instr_positive, /* TAC_POSITIVE */
    opt_eval_nop,            /* TAC_NEGATIVE */
    opt_eval_nop,            /* TAC_NOT */
    opt_eval_nop,            /* TAC_ADD */
    opt_eval_nop,            /* TAC_SUBTRACT */
    opt_eval_nop,            /* TAC_MULTIPLY */
    opt_eval_nop,            /* TAC_DIVIDE */
    opt_eval_nop,            /* TAC_CONCATENATE */
    opt_eval_nop,            /* TAC_LESS */
    opt_eval_nop,            /* TAC_LESS_EQUAL */
    opt_eval_nop,            /* TAC_GREATER */
    opt_eval_nop,            /* TAC_GREATER_EQUAL */
    opt_eval_nop,            /* TAC_EQUAL */
    opt_eval_nop,            /* TAC_NOT_EQUAL */
    opt_eval_nop,            /* TAC_AND */
    opt_eval_nop,            /* TAC_OR */
};

opt_result_t optimizer_init(optimizer_t **const opt) {
	if (opt == NULL)
		return OPT_BAD_ARG;

	/* Create a new optimizer object */
	optimizer_t *new_optimizer = malloc(sizeof(*new_optimizer));
	if (new_optimizer == NULL)
		return OPT_BAD_ALLOC;
	new_optimizer->processed_list = NULL;

	/* Initialize its innards */
	if (HTBL_INIT(new_optimizer->tmp_op_lut, 256) != HTBLSTAT_OK) {
		free(new_optimizer);
		return OPT_BAD_ALLOC;
	}

	*opt = new_optimizer;
	return OPT_OK;
}

opt_result_t optimizer_free(optimizer_t **const opt) {
	if (opt == NULL)
		return OPT_BAD_ARG;

	HTBL_FREE((*opt)->tmp_op_lut);
	free(*opt);
	*opt = NULL;
	return OPT_OK;
}

opt_result_t opt_get_operand_propag_type(optimizer_t *opt, opt_temp_data_t *dst,
                                         const tac_instruction_t *src_instr) {
	if (opt == NULL || dst == NULL || src_instr == NULL)
		return OPT_BAD_ARG;
	if (src_instr->opcode != TAC_MOVE)
		return OPT_INVALID_INSTRUCTION;

	/* R-value is a literal (constant) */
	if (tac_is_literal_operand(src_instr->src1.kind))
		dst->propag_type = OPT_PROP_LITERAL;

	/* R-value is a symbol (a named identifier) */
	else if (src_instr->src1.kind == TAC_OPERAND_SYMBOL)
		dst->propag_type = OPT_PROP_SYMBOL;

	/* R-value is another temporary operand */
	else if (src_instr->src1.kind == TAC_OPERAND_TEMP) {
		/* Retrieve the data of the source temporary symbol */
		opt_temp_data_t src_operand = {0};
		make_operand_ident(src_operand.str_ident, sizeof(src_operand.str_ident), &src_instr->src1, false);

		if (!HTBL_RETRIEVE(opt->tmp_op_lut, src_operand.str_ident, src_operand)) {
			fprintf(stderr,
			        "get_operand_propagation_type: assignment of an undefined temporary "
			        "operand %lu to temporary operand %lu\n",
			        src_instr->src1.value.id, src_instr->dst.value.id);
			return OPT_INVALID_INSTRUCTION;
		}

		/* Get the propagation type from the retrieved r-value */
		dst->propag_type = src_operand.propag_type;
	}
	// TODO: probably add a check for TAC_OPERAND_NONE in the above block

	return OPT_OK;
}

void instruction_shallow_copy(opt_temp_data_t *dst_data, const tac_instruction_t *src_instr) {
	if (dst_data == NULL || src_instr == NULL)
		return;

	dst_data->instr_data = *src_instr;
	dst_data->instr_data.name = NULL;
	dst_data->instr_data.prev = NULL;
	dst_data->instr_data.next = NULL;
}

opt_result_t opt_scan_temporary_operand(optimizer_t *opt, const tac_instruction_t *instruction) {
	if (opt == NULL || instruction == NULL)
		return OPT_BAD_ARG;
	opt_result_t result = OPT_OK;

	/* Looking only for temporary operands that are on the left side of an assignment instr. */
	if (!tac_is_assignment_opcode(instruction->opcode) || instruction->dst.kind != TAC_OPERAND_TEMP)
		return result;

	opt_temp_data_t new_operand = {
	    .propag_type = OPT_PROP_DYNAMIC, /* implicitly dynamic */
	    .instr_data = {0},
	};
	instruction_shallow_copy(&new_operand, instruction);
	make_operand_ident(new_operand.str_ident, sizeof(new_operand.str_ident), &instruction->dst, false);

	/* Decode the propagation type on MOVE instructions only */
	if (instruction->opcode == TAC_MOVE) {
		if ((result = opt_get_operand_propag_type(opt, &new_operand, instruction)) != OPT_OK)
			return result;
	}

	/* Add the new temporary operand to the LUT (FPGA jumpscare) */
	if (HTBL_EXISTS(opt->tmp_op_lut, new_operand.str_ident))
		result = OPT_REDEFINED_OPERAND_ID;
	if (HTBL_INSERT(opt->tmp_op_lut, new_operand.str_ident, &new_operand) != HTBLSTAT_OK) {
		return OPT_BAD_ALLOC;
	}

	return result;
}

opt_result_t opt_delete_redundant_move(optimizer_t *opt, tac_t *list, const tac_instruction_t *instr,
                                       const tac_instruction_t **next) {
	if (opt == NULL || list == NULL || instr == NULL)
		return OPT_BAD_ARG;
	if (instr->opcode != TAC_MOVE) /* Not a MOVE operation -> no removal */
		return OPT_OK;

	char opname[24] = "";
	opt_temp_data_t lut_data;
	make_operand_ident(opname, sizeof(opname), &instr->dst, false);
	if (HTBL_RETRIEVE(opt->tmp_op_lut, opname, lut_data) != HTBLSTAT_OK)
		return OPT_INVALID_INSTRUCTION;

	/* Destination temporary propgates DYNAMIC data or an EXPRESSION -> no removal */
	if (lut_data.propag_type != OPT_PROP_LITERAL && lut_data.propag_type != OPT_PROP_SYMBOL)
		return OPT_OK;

	/* Move label to the next instruction, if one exists and if it's possible */
	if (instr->next != NULL && instr->next->label == 0)
		instr->next->label = instr->label;
	/* TODO: This is a fix to an edge case where a labeled MOVE instruction assigning to an unnecessary
	 * temporary operand is immediately followed by another labeled instruction. Probably check if this is
	 * fine. */

	if (next != NULL)
		*next = instr->next; /* necessary because tac_remove invalidates the 'instr' pointer */

	if (tac_remove(list, instr) != TAC_OK)
		return OPT_INTERNAL_FAILURE;

	return OPT_OK;
}

opt_result_t opt_replace_temp_operands(optimizer_t *opt, tac_t *list, const tac_instruction_t *instr_pos) {
	if (opt == NULL || list == NULL || instr_pos == NULL)
		return OPT_BAD_ARG;

	char opname[24];
	bool will_replace =
	    (instr_pos->src1.kind == TAC_OPERAND_TEMP || instr_pos->src2.kind == TAC_OPERAND_TEMP);
	tac_instruction_t new_instr = {0};
	if (will_replace) /* can't edit in situ, because that's how tac_t works */
		new_instr = *instr_pos;

	if (instr_pos->src1.kind == TAC_OPERAND_TEMP) {
		make_operand_ident(opname, sizeof(opname), &instr_pos->src1, false);
		opt_temp_data_t lut_data;
		HTBL_RETRIEVE(opt->tmp_op_lut, opname, lut_data); /* TODO: add missing data check */

		if (lut_data.propag_type == OPT_PROP_EXPR) /* propagating a constant EXPRESSION result */
			new_instr.src1 = lut_data.instr_data.dst;
		else if (lut_data.propag_type != OPT_PROP_DYNAMIC) /* propagating either LITERAL or SYMBOL */
			new_instr.src1 = lut_data.instr_data.src1;
	}
	/* Dr. Smrčka is looking for his baseball bat again */
	if (instr_pos->src2.kind == TAC_OPERAND_TEMP) {
		make_operand_ident(opname, sizeof(opname), &instr_pos->src2, false);
		opt_temp_data_t lut_data;
		HTBL_RETRIEVE(opt->tmp_op_lut, opname, lut_data); /* TODO: add missing data check */

		if (lut_data.propag_type == OPT_PROP_EXPR) /* propagating a constant EXPRESSION result */
			new_instr.src2 = lut_data.instr_data.dst;
		else if (lut_data.propag_type != OPT_PROP_DYNAMIC) /* propagating either LITERAL or SYMBOL */
			new_instr.src2 = lut_data.instr_data.src1;
	}

	/* Replacing the instruction */
	if (will_replace && tac_replace(list, instr_pos, &new_instr) != TAC_OK) {
		fprintf(stderr, "opt_replace_temp_operands: failed to replace instruction at %p\n",
		        (void *)instr_pos);
		return OPT_INTERNAL_FAILURE;
	}

	return OPT_OK;
}

opt_result_t opt_evaluate_const_expression(optimizer_t *opt, tac_t *list, const tac_instruction_t *instr_pos,
                                           const tac_instruction_t **next) {
	if (opt == NULL || list == NULL || instr_pos == NULL)
		return OPT_BAD_ARG;
	opt_result_t result = OPT_OK;
	if (instr_pos->opcode == TAC_MOVE || !tac_is_assignment_opcode(instr_pos->opcode))
		return result;
	if (instr_pos->dst.kind != TAC_OPERAND_TEMP)
		return result;

	/* Both source operands must be literals */
	if (!tac_is_literal_operand(instr_pos->src1.kind) || !tac_is_literal_operand(instr_pos->src2.kind))
		return result;

	opt_temp_data_t new_operand = {
	    .propag_type = OPT_PROP_EXPR,
	    .instr_data = {0},
	    .str_ident = "",
	};
	make_operand_ident(new_operand.str_ident, sizeof(new_operand.str_ident), &instr_pos->dst, false);

	/* Determine the evaluator function for this operation */
	int evaluator_idx = (instr_pos->opcode - TAC_POSITIVE);
	if (evaluator_idx < 0 || evaluator_idx >= (long)sizeof(EXPRESSION_EVALS)) {
		fprintf(stderr, "opt_evaluate_const_expression: evaluator index %d out of bounds [0, %ld)\n",
		        evaluator_idx, (long)sizeof(EXPRESSION_EVALS));
		return OPT_INTERNAL_FAILURE;
	}

	expression_evaluator_t eval_func = EXPRESSION_EVALS[evaluator_idx];
	/* Do nothing if false is returned (see expression_evaluator_t doc comment) */
	if (!eval_func(&new_operand.instr_data.dst, &instr_pos->src1, &instr_pos->src2))
		return OPT_OK;

	if (HTBL_EXISTS(opt->tmp_op_lut, new_operand.str_ident))
		result = OPT_REDEFINED_OPERAND_ID;
	if (HTBL_INSERT(opt->tmp_op_lut, new_operand.str_ident, &new_operand) != HTBLSTAT_OK)
		return OPT_BAD_ALLOC;

	/* What if you went through the trouble of raising a child, from its infancy to a brilliant young man,
	 * only for him to be removed from existence? Well that's what's happening here. */
	if (next != NULL)
		*next = instr_pos->next;
	if (tac_remove(list, instr_pos) != TAC_OK)
		return OPT_INTERNAL_FAILURE;

	return result;
}

opt_result_t optimizer_first_pass(optimizer_t *opt, tac_t *tac) {
	if (opt == NULL || tac == NULL)
		return OPT_BAD_ARG;

	opt_result_t result = OPT_OK;
	opt->processed_list = tac; /* save this list to let the next pass know it's been worked on */

	/* Iterating through the entire instruction array */
	const tac_instruction_t *cur_instr = tac_first(tac);
	while (cur_instr != NULL) {
		if ((result = opt_scan_temporary_operand(opt, cur_instr)) != OPT_OK) {
			fprintf(stderr, "optimizer_first_pass: failed to scan temporary operand\n");
			return result;
		}
		/* This is setup so that it's possible to add several scanning functions here in the future */

		cur_instr = cur_instr->next;
	}

	return result;
}

opt_result_t optimizer_second_pass(optimizer_t *opt, tac_t *tac) {
	if (opt == NULL || opt->processed_list == NULL || tac == NULL)
		return OPT_BAD_ARG;
	if (opt->processed_list != tac)
		return OPT_INVALID_LIST;

	/* Iterating through the array again */
	opt_result_t result = OPT_OK;
	const tac_instruction_t *cur_instr = tac_first(tac);
	while (cur_instr != NULL) {
		const tac_instruction_t *instr_skip = NULL;

		/* Redundant MOVE instruction deletion */
		if ((result = opt_delete_redundant_move(opt, tac, cur_instr, &instr_skip)) != OPT_OK)
			fprintf(stderr, "optimizer_pass: failed to delete instruction at %p (fail code %d)\n",
			        (void *)cur_instr, result);
		if (instr_skip != NULL) { /* you wouldn't want to optimize a black hole, would you? */
			cur_instr = instr_skip;
			continue;
		}

		/* Temporary operand forwarding (propagation, replacement, whatever you fancy) */
		if ((result = opt_replace_temp_operands(opt, tac, cur_instr)) != OPT_OK)
			fprintf(stderr,
			        "optimizer_pass: failed to replace temporary operands of instruction %p (code %d)\n",
			        (void *)cur_instr, result);

		/* Constant expression evaluation */
		if ((result = opt_evaluate_const_expression(opt, tac, cur_instr, &instr_skip)) != OPT_OK)
			fprintf(stderr, "optimizer_pass: failed to evaluate expression of instruction %p (code %d)\n",
			        (void *)cur_instr, result);
		if (instr_skip != NULL) {
			cur_instr = instr_skip;
			continue;
		}

		cur_instr = cur_instr->next;
	}

	opt->processed_list = NULL;
	return result;
}
