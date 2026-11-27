/**
   @file codegen.c
   @author xbosakd00 (DasBean125)
   @date 2026-11-23
   @brief The target code (IFJcode26) generator module.
*/

/* codegen.c becomes the source of extern inline definitions */
#define UNIT_UNDER_TEST
#define IFJ_CODEGEN_EXTERN_PROVIDER
#include "include/codegen_impl.h"
#include "include/codegen_shared.h"

#include <stdlib.h>
#include <string.h>

/*================
  Public functions
  ================*/

cdg_result_t codegen_init(codegen_t **gen, funct_t *func_table) {
	if (gen == NULL || func_table == NULL)
		return CDG_BAD_ARG;

	codegen_t *new_codegen = malloc(sizeof(*new_codegen));
	if (new_codegen == NULL)
		return CDG_BAD_ALLOC;

	/* Initialize the machine */
	*new_codegen = (codegen_t){0};

	if (HTBL_INIT(new_codegen->addr_table, 64) != HTBLSTAT_OK) {
		free(new_codegen);
		return CDG_BAD_ALLOC;
	}
	new_codegen->func_table = func_table;
	new_codegen->in_local_scope = false;
	new_codegen->stream = stdout;

	*gen = new_codegen;
	return CDG_OK;
}

cdg_result_t codegen_free(codegen_t **gen) {
	if (gen == NULL)
		return CDG_BAD_ARG;

	HTBL_FREE((*gen)->addr_table);
	free((*gen)->param_stack);
	free(*gen);
	*gen = NULL;
	return CDG_OK;
}

cdg_result_t codegen_run(FILE *stream, codegen_t *gen, const tac_t *code) {
	if (code == NULL || gen == NULL)
		return CDG_BAD_ARG;
	/* This is mainly useful for testing */
	if (stream != NULL)
		gen->stream = stream;

	/* IFJcode26 header */
	cdg_print_instr_direct(gen, ".IFJcode26", CDG_PFLAG_APPEND_LINE_FEED);
	cdg_print_instr_direct(gen, "JUMP _main\n", CDG_PFLAG_APPEND_LINE_FEED);

	/* Run the machine of doom and despair */
	return codegen_generate_code(gen, code);
}

/*==================
  Internal functions
  ==================*/

cdg_result_t cdg_alloc_param_stack(codegen_t *gen, const size_t size) {
	if (gen == NULL || size == 0)
		return CDG_BAD_ARG;
	if (size >= SIZE_MAX) {
		fprintf(stderr, "cdg_alloc_param_stack: the stack has reached the stratosphere (size %lu)\n", size);
		return CDG_BAD_ALLOC;
	}
	if (size == gen->pstack_size) /* Nyet, stack is fine */
		return CDG_OK;

	tac_operand_t *new_stack = realloc(gen->param_stack, size * sizeof(*new_stack));
	if (new_stack == NULL)
		return CDG_BAD_ALLOC;

	gen->pstack_size = size;
	gen->param_stack = new_stack;
	return CDG_OK;
}

cdg_result_t cdg_clear_param_stack(codegen_t *gen) {
	if (gen == NULL)
		return CDG_BAD_ARG;
	if (gen->param_stack == NULL) /* nothing to clear */
		return CDG_OK;

	/* Yeah this is copying but like... whatever */
	static const tac_operand_t EMPTY_OPERAND = {
	    .kind = TAC_OPERAND_NONE, .type = SYMT_TYPE_UNDEFINED, .value.integer = 0};

	for (unsigned i = 0; i < gen->pstack_size; i++) {
		gen->param_stack[i] = EMPTY_OPERAND;
	}
	gen->pstack_count = 0;
	return CDG_OK;
}

cdg_result_t cdg_append_param(codegen_t *gen, const tac_operand_t *op) {
	if (gen == NULL || op == NULL)
		return CDG_BAD_ARG;

	/* Give me your RAM, and your CPU, and your PSU, and your motherboard, and yo- */
	if (gen->pstack_count >= gen->pstack_size) {
		const size_t new_size = gen->pstack_size ? gen->pstack_size * 2 : 4;
		if (cdg_alloc_param_stack(gen, new_size) != CDG_OK)
			return CDG_BAD_ALLOC;
	}

	/* Insert */
	gen->param_stack[gen->pstack_count] = *op;
	gen->pstack_count++;
	return CDG_OK;
}

/*=======================
  Code printing functions
  =======================*/

cdg_result_t cdg_get_symbol_address(codegen_t *gen, cdg_address_t *dst, const tac_operand_t *op,
                                    bool is_internal) {
	if (gen == NULL || dst == NULL)
		return CDG_BAD_ARG;
	if (op->kind != TAC_OPERAND_TEMP && op->kind != TAC_OPERAND_SYMBOL)
		return CDG_OK;

	char op_ident[24];
	if (!make_operand_ident(op_ident, sizeof(op_ident), op, is_internal))
		return CDG_BAD_ALLOC;

	/* Symbol has a record already */
	if (HTBL_RETRIEVE(gen->addr_table, op_ident, *dst))
		return CDG_OK;

	/* Create a new symbol and store it */
	*dst = (cdg_address_t){0};
	dst->type = op->kind;
	dst->is_local = gen->in_local_scope;

	if (is_internal) /* Internal operands are those defined by the codegen */
		dst->code_id = gen->int_temp_acc++;
	else { /* Otherwise this is an instruction defined by the previous translator stage */
		if (dst->is_local)
			dst->code_id = (op->kind == TAC_OPERAND_SYMBOL ? gen->loc_sym_acc++ : gen->loc_temp_acc++);
		else
			dst->code_id = (op->kind == TAC_OPERAND_SYMBOL ? gen->sym_acc++ : gen->temp_acc++);
	}

	if (HTBL_INSERT(gen->addr_table, op_ident, dst) != HTBLSTAT_OK)
		return CDG_BAD_ALLOC;

	return CDG_OK;
}

cdg_result_t cdg_derive_literal_from_symbol(tac_operand_t *dst, const tac_operand_t *src) {
	if (dst == NULL || src == NULL)
		return CDG_BAD_ARG;

	/* Maps a symt_data_type_t value to a tac_operand_kind_t value */
	/* This mapping function is injective because at most one element from symt_data_type_t has an image
	in tac_operand_kint_t (IDM jumpscare) */
	static const tac_operand_kind_t TYPE_MAP[] = {
	    TAC_OPERAND_UNIT,   TAC_OPERAND_INTEGER, TAC_OPERAND_DOUBLE,
	    TAC_OPERAND_STRING, TAC_OPERAND_BOOLEAN, TAC_OPERAND_NONE,
	};

	dst->kind = TYPE_MAP[src->type];
	dst->type = src->type;
	return CDG_OK;
}

cdg_result_t cdg_print_literal(codegen_t *gen, const tac_operand_t *op) {
	cdg_result_t result = CDG_OK;

	/* Note how every keyword takes up exactly 7 chars (except last one, including terminator) */
	static constexpr char LITERAL_TYPE_IDENTS[] = "int\0   float\0 string\0bool\0  nil";
	static constexpr int LITERAL_IDENT_ALIGNMENT = 7;

	int data_type_idx = (op->kind - TAC_OPERAND_INTEGER);
	if (data_type_idx < 0 || data_type_idx > 4) {
		fprintf(stderr, "codegen_print_literal: Operand type %d is out of bounds [0,4]\n", op->kind);
		return CDG_INTERNAL_FAILURE;
	}

	/* Derive the storage type (int, float, etc.) based on the operand kind */
	fprintf(gen->stream, "%s@", &LITERAL_TYPE_IDENTS[data_type_idx * LITERAL_IDENT_ALIGNMENT]);

	switch (op->type) {
	case SYMT_TYPE_UNIT:
		fprintf(gen->stream, "nil");
		break;
	case SYMT_TYPE_INTEGER:
		fprintf(gen->stream, "%ld", op->value.integer);
		break;
	case SYMT_TYPE_DOUBLE:
		fprintf(gen->stream, "%a", op->value.decimal);
		break;
	case SYMT_TYPE_BOOLEAN:
		fprintf(gen->stream, op->value.boolean ? "true" : "false");
		break;
	case SYMT_TYPE_STRING:
		for (const char *c = op->value.string; *c != '\0'; c++) {
			/* Special character escapes */
			if ((*c >= 0 && *c <= 32) || *c == 35 || *c == 92) {
				fprintf(gen->stream, "\\%03d", *c);
				continue;
			}

			/* Normal character is simply written as-is */
			fputc(*c, gen->stream);
		}
		break;
	default:
		fprintf(gen->stream, "nil@nil");
		fprintf(stderr, "codegen_print_operand: Data type %d is not defined in IFJcode26\n", op->type);
		result = CDG_INVALID_OPERAND;
		break;
	}

	return result;
}

cdg_result_t cdg_print_operand(codegen_t *gen, const tac_operand_t *op, int flags) {
	if (gen == NULL || op == NULL)
		return CDG_BAD_ARG;
	if (op->kind == TAC_OPERAND_NONE) /* nuh uh */
		return CDG_INVALID_OPERAND;
	cdg_result_t result = CDG_OK;

	/* LITERAL operands */
	if (tac_is_literal_operand(op->kind)) {
		result = cdg_print_literal(gen, op);
		goto PRINT_OK;
	}

	/* SYMBOL operands */
	/* Identifiers: temporary - t#, symbols - $#, internal temporary - it# */
	const bool ass_internal = (flags & CDG_PFLAG_INTERNAL_NAMESPACE);
	if (ass_internal && op->kind == TAC_OPERAND_SYMBOL) /* only temporary internals allowed */
		return CDG_INVALID_OPERAND;

	cdg_address_t op_address;
	if (cdg_get_symbol_address(gen, &op_address, op, ass_internal) != CDG_OK) {
		fprintf(stderr, "codegen_print_operand: Operand [K,T,ID]=[%d,%d,%lu] has no address information\n",
		        op->kind, op->type, op->value.id);
		return CDG_INTERNAL_FAILURE;
	}

	/* Print the symbol in appropriate format (<FRAME>@<name>) */
	const char *frame_id = (op_address.is_local ? "LF" : "GF");
	frame_id = ((flags & CDG_PFLAG_DECL_TEMP) ? "TF" : frame_id);
	const char name_prefix = (op_address.type == TAC_OPERAND_TEMP ? 't' : '$');

	/* Liar, lawyer, mirror for ya, what's the difference? (in the string) */
	char *fmt = ass_internal ? "%s@%ci%lu" : "%s@%c%lu";
	fprintf(gen->stream, fmt, frame_id, name_prefix, op_address.code_id);

PRINT_OK:
	/* Special terminator symbols */
	if (flags & CDG_PFLAG_APPEND_SPACE)
		fputc(' ', gen->stream);
	if (flags & CDG_PFLAG_APPEND_LINE_FEED)
		fputc('\n', gen->stream);

	return result;
}

cdg_result_t cdg_print_defvar(codegen_t *gen, const tac_operand_t *op, bool is_internal) {
	if (gen == NULL || op == NULL)
		return CDG_BAD_ARG;
	if (!tac_is_symbol_operand(op->kind))
		return CDG_INVALID_OPERAND;
	cdg_result_t result = CDG_OK;

	/* Determine if the operand has been defined already */
	cdg_address_t addr;
	if (cdg_get_symbol_address(gen, &addr, op, is_internal) != CDG_OK)
		return CDG_INVALID_OPERAND;
	if (addr.is_defined) /* already defined, no need to do anything */
		return CDG_OK;

	/* Define the operand and save this information in the address table */
	cdg_print_instr_direct(gen, "DEFVAR", CDG_PFLAG_APPEND_SPACE);
	int flags = (is_internal * CDG_PFLAG_INTERNAL_NAMESPACE) | CDG_PFLAG_APPEND_LINE_FEED;
	if ((result = cdg_print_operand(gen, op, flags)) != CDG_OK)
		return result;

	/* Write back the same address, but with the is_defined field set to 'true' */
	addr.is_defined = true;
	char op_ident[24];
	if (!make_operand_ident(op_ident, sizeof(op_ident), op, is_internal))
		return CDG_BAD_ALLOC;

	/* See, if we could return pointers from the hashtable module, this would be efficient, but
	   "REEEEE mUh AbStRACt dAtA TyPE!!!1!! */
	if (HTBL_INSERT(gen->addr_table, op_ident, &addr) != HTBLSTAT_OK)
		return CDG_BAD_ALLOC;
	return result;
}

cdg_result_t cdg_print_label(codegen_t *gen, const tac_instruction_t *instr, bool is_func, int flags) {
	if (gen == NULL || instr == NULL)
		return CDG_BAD_ARG;

	/* Function label format: _<func_name> */
	bool ass_internal = (flags & CDG_PFLAG_INTERNAL_NAMESPACE);
	if (is_func && !ass_internal) {
		fprintf(gen->stream, "_%s", instr->name);
		goto PRINT_OK;
	}

	/* Normal label format: &#, internal label format: &i# */
	if (ass_internal) {
		gen->int_label_acc += (flags & CDG_PFLAG_NEW_LABEL ? 1 : 0);
		fprintf(gen->stream, "&i%u", gen->int_label_acc);
	} else
		fprintf(gen->stream, "&%lu", instr->label);

PRINT_OK:
	/* Special terminator symbols */
	if (flags & CDG_PFLAG_APPEND_SPACE)
		fputc(' ', gen->stream);
	if (flags & CDG_PFLAG_APPEND_LINE_FEED)
		fputc('\n', gen->stream);

	return CDG_OK;
}

/*========================
  Let the generation begin (the code generator functions follow)
  ========================*/
/**
   @brief Generate the start of a function definition.

   Resets local temporary and symbol counters, creates a label identifying the function and probably some
   other cool stuff. For param info, see @ref generator_func_t.
*/
cdg_result_t cdg_gen_function_begin(codegen_t *gen, const tac_instruction_t *tac) {
	if (tac->opcode != TAC_FUNCTION_BEGIN)
		return CDG_INVALID_INSTRUCTION;
	cdg_result_t result = CDG_INVALID_INSTRUCTION;

	if (gen->in_local_scope)
		fprintf(stderr, "cdg_gen_function_begin: generating FUNCTION_BEGIN instruction while in local scope "
		                "is forbidden\n");

	/* Function label */
	cdg_print_instr_direct(gen, "LABEL", CDG_PFLAG_APPEND_SPACE);
	if ((result = cdg_print_label(gen, tac, CDG_LABEL_FUNC, CDG_PFLAG_APPEND_LINE_FEED)) != CDG_OK)
		return result;

	/* Define (declare is a more correct term here) the return value at the beginning of the function.
	   This is used by RETURN later. */
	cdg_print_instr_direct(gen, "DEFVAR LF@return", CDG_PFLAG_APPEND_LINE_FEED);

	/* Reset local symbol ID accumulators and enter local scope */
	gen->loc_sym_acc = 0;
	gen->loc_temp_acc = 0;
	gen->in_local_scope = true;
	return result;
}

/**
   @brief Generate the end of a function definition.

   Doesn't actually generate any code, but it is required for proper state handling. For param info, see
   @ref generator_func_t.
*/
cdg_result_t cdg_gen_function_end(codegen_t *gen, const tac_instruction_t *tac) {
	if (tac->opcode != TAC_FUNCTION_END)
		return CDG_INVALID_INSTRUCTION;

	/* Exit local scope, since we've left the function body */
	gen->in_local_scope = false;
	return CDG_OK;
}

/**
   @brief Register a PARAM instruction for generation.

   Because @ref cdg_gen_func_arg writes arguments in order, this function has to read them in reverse
   order. It uses the param_stack field of the code generator to do this. For param info, see
   @ref generator_func_t.
*/
cdg_result_t cdg_register_func_param(codegen_t *gen, const tac_instruction_t *tac) {
	if (tac->opcode != TAC_PARAM)
		return CDG_INVALID_INSTRUCTION;

	/* TAC_PARAM stores the operand in .dst */
	return cdg_append_param(gen, &tac->dst);
}

/**
   @brief Generate a series of PARAM instructions stored in the parameter stack.

   This reads instructions defined in param_stack of the code generator struct in reverse order to comply with
   stack semantics.
   @param gen The code generator to generate from.
   @returns CDG_OK on success or any non-OK return code on failure. Return semantics from
   @ref generator_func_t apply here as well, even though this function has a different type.
*/
cdg_result_t cdg_generate_func_params(codegen_t *gen) {
	cdg_result_t result = CDG_OK;
	if (gen->pstack_count == 0)
		return result;

	/* Generate POPS instructions in reverse order */
	for (int i = (int)gen->pstack_count - 1; i >= 0; i--) {
		if ((result = cdg_print_defvar(gen, &gen->param_stack[i], false)) != CDG_OK)
			break;

		cdg_print_instr_direct(gen, "POPS", CDG_PFLAG_APPEND_SPACE);
		if ((result = cdg_print_operand(gen, &gen->param_stack[i], CDG_PFLAG_APPEND_LINE_FEED)) != CDG_OK)
			break;
	}
	if ((result = cdg_clear_param_stack(gen)) != CDG_OK)
		return result;

	return result;
}

/**
   @brief Generate a MOVE instruction. For param info, see @ref generator_func_t.
*/
cdg_result_t cdg_gen_move(codegen_t *gen, const tac_instruction_t *tac) {
	if (tac->opcode != TAC_MOVE)
		return CDG_INVALID_INSTRUCTION;
	cdg_result_t result = CDG_OK;

	/* Make sure the destination is defined */
	if ((result = cdg_print_defvar(gen, &tac->dst, false)) != CDG_OK)
		return result;

	/* Print the symbolator */
	fprintf(gen->stream, "MOVE ");
	if ((result = cdg_print_operand(gen, &tac->dst, CDG_PFLAG_APPEND_SPACE)) != CDG_OK) /* dst */
		return result;
	return cdg_print_operand(gen, &tac->src1, CDG_PFLAG_APPEND_LINE_FEED); /* src */
}

/**
   @brief Generate a POSITIVE instruction. For param info, see @ref generator_func_t.
*/
cdg_result_t cdg_gen_positive(codegen_t *gen, const tac_instruction_t *tac) {
	if (tac->opcode != TAC_POSITIVE)
		return CDG_INVALID_INSTRUCTION;
	cdg_result_t result = CDG_OK;

	/* POSITIVE is essentially just 'MOVE dst +src', where src is a numeric literal */
	if ((result = cdg_print_defvar(gen, &tac->dst, false)) != CDG_OK)
		return result;
	cdg_print_instr_direct(gen, "MOVE", CDG_PFLAG_APPEND_SPACE);
	if ((result = cdg_print_operand(gen, &tac->dst, CDG_PFLAG_APPEND_SPACE)) != CDG_OK) /* dst */
		return result;

	/* Make sure the source is actually a numeric literal */
	if (!tac_is_numeric_literal_operand(tac->src1.kind))
		return CDG_INVALID_OPERAND;

	/* Convert to positive numeric value and print it */
	tac_operand_t positive_operand = tac->src1;
	if (positive_operand.type == SYMT_TYPE_DOUBLE)
		positive_operand.value.decimal = +positive_operand.value.decimal;
	else
		positive_operand.value.integer = +positive_operand.value.integer;
	if ((result = cdg_print_operand(gen, &positive_operand, CDG_PFLAG_APPEND_LINE_FEED)) != CDG_OK) /* +src */
		return result;

	return result;
}

/**
   @brief Generate a NEGATIVE instruction. For param info, see @ref generator_func_t.
*/
cdg_result_t cdg_gen_negative(codegen_t *gen, const tac_instruction_t *tac) {
	if (tac->opcode != TAC_NEGATIVE)
		return CDG_INVALID_INSTRUCTION;
	cdg_result_t result = CDG_INVALID_INSTRUCTION;

	if ((result = cdg_print_defvar(gen, &tac->dst, false)) != CDG_OK)
		return result;

	/* If src is a numeric literal, simply derive its negative value beforehand */
	if (tac_is_numeric_literal_operand(tac->src1.kind)) {
		cdg_print_instr_direct(gen, "MOVE", CDG_PFLAG_APPEND_SPACE);
		if ((result = cdg_print_operand(gen, &tac->dst, CDG_PFLAG_APPEND_SPACE)) != CDG_OK) /* dst */
			return result;

		tac_operand_t negative_operand = tac->src1;
		if (negative_operand.type == SYMT_TYPE_DOUBLE)
			negative_operand.value.decimal = -negative_operand.value.decimal;
		else
			negative_operand.value.integer = -negative_operand.value.integer;
		result = cdg_print_operand(gen, &negative_operand, CDG_PFLAG_APPEND_LINE_FEED); /* -src */

		/* If src is a symbol, we have to multiply by -1 */
	} else if (tac_is_symbol_operand(tac->src1.kind)) {
		cdg_print_instr_direct(gen, "MUL", CDG_PFLAG_APPEND_SPACE);
		if ((result = cdg_print_operand(gen, &tac->dst, CDG_PFLAG_APPEND_SPACE)) != CDG_OK) /* dst */
			return result;
		if ((result = cdg_print_operand(gen, &tac->src1, CDG_PFLAG_APPEND_SPACE)) != CDG_OK) /* src */
			return result;

		/* Create a multiplier of the appropriate data type (float or int) */
		tac_operand_t multiplier;
		if ((result = cdg_derive_literal_from_symbol(&multiplier, &tac->src1)) != CDG_OK)
			return result;
		if (multiplier.type == SYMT_TYPE_DOUBLE)
			multiplier.value.decimal = -1;
		else
			multiplier.value.integer = -1;
		result = cdg_print_operand(gen, &multiplier, CDG_PFLAG_APPEND_LINE_FEED); /* mult */
	}

	return result;
}

/**
   @brief Generate a NOT instruction. For param info, see @ref generator_func_t.
*/
cdg_result_t cdg_gen_not(codegen_t *gen, const tac_instruction_t *tac) {
	if (tac->opcode != TAC_NEGATIVE)
		return CDG_INVALID_INSTRUCTION;
	cdg_result_t result = CDG_INVALID_INSTRUCTION;

	if ((result = cdg_print_defvar(gen, &tac->dst, false)) != CDG_OK)
		return result;
	cdg_print_instr_direct(gen, "NOT", CDG_PFLAG_APPEND_SPACE);
	if ((result = cdg_print_operand(gen, &tac->dst, CDG_PFLAG_APPEND_SPACE)) != CDG_OK) /* dst */
		return result;
	return cdg_print_operand(gen, &tac->src1, CDG_PFLAG_APPEND_LINE_FEED); /* src */
}

/**
   @brief A combined generator printing one instruction from the set {ADD, SUB, MULTIPLY, CONCATENATE,
   AND, OR, LESS, GREATER, EQUAL}.

   For param info, see @ref generator_func_t.
*/
cdg_result_t cdg_gen_simple_binary(codegen_t *gen, const tac_instruction_t *tac) {
	cdg_result_t result = CDG_INVALID_INSTRUCTION;
	char *instruction = "";

	/* Determine the instruction type */
	switch (tac->opcode) {
	case TAC_ADD:
		instruction = "ADD";
		break;
	case TAC_SUBTRACT:
		instruction = "SUB";
		break;
	case TAC_MULTIPLY:
		instruction = "MUL";
		break;
	case TAC_CONCATENATE:
		instruction = "CONCAT";
		break;
	case TAC_AND:
		instruction = "AND";
		break;
	case TAC_OR:
		instruction = "OR";
		break;
	case TAC_LESS:
		instruction = "LT";
		break;
	case TAC_GREATER:
		instruction = "GT";
		break;
	case TAC_EQUAL:
		instruction = "EQ";
		break;
	default:
		return result;
	}

	/* Mmmmsoup */
	if ((result = cdg_print_defvar(gen, &tac->dst, false)) != CDG_OK)
		return result;

	cdg_print_instr_direct(gen, instruction, CDG_PFLAG_APPEND_SPACE);
	if ((result = cdg_print_operand(gen, &tac->dst, CDG_PFLAG_APPEND_SPACE)) != CDG_OK) /* dst */
		return result;
	if ((result = cdg_print_operand(gen, &tac->src1, CDG_PFLAG_APPEND_SPACE)) != CDG_OK) /* left src */
		return result;
	return cdg_print_operand(gen, &tac->src2, CDG_PFLAG_APPEND_LINE_FEED); /* right src */
}

/**
   @brief Generate a DIVIDE instruction.

   Automatically differentiates between integer and floating-point division.
   For param info, see @ref generator_func_t.
*/
cdg_result_t cdg_gen_divide(codegen_t *gen, const tac_instruction_t *tac) {
	if (tac->opcode != TAC_DIVIDE)
		return CDG_INVALID_INSTRUCTION;
	cdg_result_t result = CDG_INVALID_INSTRUCTION;

	/* Determine whether we'll be dividing integers or floats */
	char *instruction = "DIV"; /* happy  floating */
	if (tac->src1.kind == TAC_OPERAND_INTEGER && tac->src2.kind == TAC_OPERAND_INTEGER)
		instruction = "IDIV";

	/* Print that s##t */
	if ((result = cdg_print_defvar(gen, &tac->dst, false)) != CDG_OK)
		return result;

	cdg_print_instr_direct(gen, instruction, CDG_PFLAG_APPEND_SPACE);
	if ((result = cdg_print_operand(gen, &tac->dst, CDG_PFLAG_APPEND_SPACE)) != CDG_OK) /* dst */
		return result;
	if ((result = cdg_print_operand(gen, &tac->src1, CDG_PFLAG_APPEND_SPACE)) != CDG_OK) /* left src */
		return result;
	return cdg_print_operand(gen, &tac->src2, CDG_PFLAG_APPEND_LINE_FEED); /* right src */
}

/**
   @brief Generate a non-strict inequality instruction, i.e. LESS/GREATER THAN or EQUAL TO.

   This is a series of instructions due to the nature of IFJcode26. For param info, see
   @ref generator_func_t.
*/
cdg_result_t cdg_gen_nonstrict_inequality(codegen_t *gen, const tac_instruction_t *tac) {
	if (tac->opcode != TAC_LESS_EQUAL && tac->opcode != TAC_GREATER_EQUAL)
		return CDG_INVALID_INSTRUCTION;
	cdg_result_t result = CDG_INVALID_INSTRUCTION;

	/* Greater than/less than determination */
	char *main_instr = "GT"; /* <=> !(LT or EQ) */
	if (tac->opcode == TAC_GREATER_EQUAL)
		main_instr = "LT"; /* <=> !(GT or EQ) */

	tac_operand_t temp_dst = tac->dst;
	temp_dst.kind = TAC_OPERAND_TEMP;

	/* INEQUALITY part (saves to an internal temporary destination) */
	/* NOTE: cdg_print_operand modifies gen->int_temp_acc with the CDG_PFLAG_ASSUME_INTERNAL. This is an
	   insane side-effect that will definitely cause issues I cannot foresee, but it should probably work. */
	temp_dst.value.id = gen->int_temp_acc;
	if ((result = cdg_print_defvar(gen, &temp_dst, true)) != CDG_OK)
		return result;

	cdg_print_instr_direct(gen, main_instr, CDG_PFLAG_APPEND_SPACE);
	if ((result = cdg_print_operand(gen, &temp_dst, CDG_PFLAG_APPEND_SPACE | CDG_PFLAG_INTERNAL_NAMESPACE)) !=
	    CDG_OK) /* temp_dst */
		return result;
	if ((result = cdg_print_operand(gen, &tac->src1, CDG_PFLAG_APPEND_SPACE)) != CDG_OK) /* left src */
		return result;
	if ((result = cdg_print_operand(gen, &tac->src2, CDG_PFLAG_APPEND_LINE_FEED)) != CDG_OK) /* right src */
		return result;

	/* NOT part */
	if ((result = cdg_print_defvar(gen, &tac->dst, false)) != CDG_OK)
		return result;
	cdg_print_instr_direct(gen, "NOT", CDG_PFLAG_APPEND_SPACE);
	if ((result = cdg_print_operand(gen, &tac->dst, CDG_PFLAG_APPEND_SPACE)) != CDG_OK) /* dst */
		return result;
	return cdg_print_operand(gen, &temp_dst, CDG_PFLAG_APPEND_LINE_FEED | CDG_PFLAG_INTERNAL_NAMESPACE);
}

/**
   @brief Generate a NOT EQUAL instruction.

   This is a series of instructions, because I just LOVE IFJcode26. For param info, see @ref generator_func_t.
*/
cdg_result_t cdg_gen_not_equal(codegen_t *gen, const tac_instruction_t *tac) {
	if (tac->opcode != TAC_EQUAL)
		return CDG_INVALID_INSTRUCTION;
	cdg_result_t result = CDG_INVALID_INSTRUCTION;

	/* More room here than in cdg_gen_nonstrict_inequality so I'll try to explain this: we need a
	   temporary variable in which to store the partial results. This destination has the same type as the
	   "true" destination, so we simply copy it over, then force it to a TEMP and then set its id to a
	   unique number. */
	tac_operand_t temp_dst = tac->dst;
	temp_dst.kind = TAC_OPERAND_TEMP;

	temp_dst.value.id = gen->int_temp_acc;
	if ((result = cdg_print_defvar(gen, &temp_dst, true)) != CDG_OK)
		return result;

	/* EQUAL part (saved to an internal temporary) */
	cdg_print_instr_direct(gen, "EQ", CDG_PFLAG_APPEND_SPACE);
	if ((result = cdg_print_operand(gen, &temp_dst, CDG_PFLAG_APPEND_SPACE | CDG_PFLAG_INTERNAL_NAMESPACE)) !=
	    CDG_OK) /* temp_dst */
		return result;
	if ((result = cdg_print_operand(gen, &tac->src1, CDG_PFLAG_APPEND_SPACE)) != CDG_OK) /* left src */
		return result;
	if ((result = cdg_print_operand(gen, &tac->src2, CDG_PFLAG_APPEND_LINE_FEED)) != CDG_OK) /* right src */
		return result;

	/* NOT part */
	if ((result = cdg_print_defvar(gen, &tac->dst, true)) != CDG_OK)
		return result;
	cdg_print_instr_direct(gen, "NOT", CDG_PFLAG_APPEND_SPACE);
	if ((result = cdg_print_operand(gen, &tac->dst, CDG_PFLAG_APPEND_SPACE)) != CDG_OK) /* dst */
		return result;
	return cdg_print_operand(gen, &temp_dst,
	                         CDG_PFLAG_APPEND_LINE_FEED | CDG_PFLAG_INTERNAL_NAMESPACE); /* temp src */
}

/**
   @brief Generate a function argument push instruction.

   Arguments are passed through the stack and pushed _in order_ (i.e., left-to-right).
   For param info, see @ref generator_func_t.
*/
cdg_result_t cdg_gen_func_arg(codegen_t *gen, const tac_instruction_t *tac) {
	if (tac->opcode != TAC_ARG)
		return CDG_INVALID_INSTRUCTION;

	/* Push operand onto stack */
	cdg_print_instr_direct(gen, "PUSHS", CDG_PFLAG_APPEND_SPACE);
	return cdg_print_operand(gen, &tac->src1, CDG_PFLAG_APPEND_LINE_FEED);
}

/**
   @brief Generate a function call instruction.

   Parametrized functions must have their arguments defined by @ref cdg_gen_func_arg.
   For param info, see @ref generator_func_t.
*/
cdg_result_t cdg_gen_func_call(codegen_t *gen, const tac_instruction_t *tac) {
	if (tac->opcode != TAC_CALL)
		return CDG_INVALID_INSTRUCTION;
	cdg_result_t result = CDG_INVALID_INSTRUCTION;

	/* Initialize a new local frame */
	cdg_print_instr_direct(gen, "CREATEFRAME", CDG_PFLAG_APPEND_LINE_FEED);
	cdg_print_instr_direct(gen, "PUSHFRAME", CDG_PFLAG_APPEND_LINE_FEED);

	/* Add CALL instruction, followed by return value write to the TAC_CALL's destination */
	cdg_print_instr_direct(gen, "CALL", CDG_PFLAG_APPEND_SPACE);
	if ((result = cdg_print_label(gen, tac, CDG_LABEL_FUNC, CDG_PFLAG_APPEND_LINE_FEED)) != CDG_OK)
		return result;

	cdg_print_instr_direct(gen, "MOVE", CDG_PFLAG_APPEND_SPACE);
	if ((result = cdg_print_operand(gen, &tac->dst, CDG_PFLAG_APPEND_SPACE)) != CDG_OK)
		return result;
	cdg_print_instr_direct(gen, "TF@return", CDG_PFLAG_APPEND_LINE_FEED);
	return result;
}

/**
   @brief Generate a function return instruction. For param info, see @ref generator_func_t.
*/
cdg_result_t cdg_gen_func_return(codegen_t *gen, const tac_instruction_t *tac) {
	if (tac->opcode != TAC_RETURN)
		return CDG_INVALID_INSTRUCTION;
	cdg_result_t result = CDG_INVALID_INSTRUCTION;

	/* Store the function's result in a special var 'return' stored in the temp frame */
	cdg_print_instr_direct(gen, "POPFRAME", CDG_PFLAG_APPEND_LINE_FEED);
	cdg_print_instr_direct(gen, "MOVE TF@return", CDG_PFLAG_APPEND_SPACE);
	if ((result = cdg_print_operand(gen, &tac->src1, CDG_PFLAG_APPEND_LINE_FEED)) != CDG_OK)
		return result;

	/* Return to the call site */
	cdg_print_instr_direct(gen, "RETURN", CDG_PFLAG_APPEND_LINE_FEED);
	return CDG_OK;
}

/**
   @brief Generate a runtime non-zero integer test.

   Multi-instruction operation. For param info, see @ref generator_func_t.
*/
cdg_result_t cdg_gen_non_zero_test(codegen_t *gen, const tac_instruction_t *tac) {
	if (tac->opcode != TAC_CHECK_NONZERO)
		return CDG_INVALID_INSTRUCTION;
	cdg_result_t result = CDG_INVALID_INSTRUCTION;

	/* Reference code:
	   JUMPIFNEQ &i0 LF@s0 int@0
	   EXIT int@40
	   LABEL &i0
	*/
	cdg_print_instr_direct(gen, "JUMPIFNEQ", CDG_PFLAG_APPEND_SPACE);
	int flags = CDG_PFLAG_APPEND_SPACE | CDG_PFLAG_NEW_LABEL;
	if ((result = cdg_print_label(gen, tac, CDG_LABEL_GENERIC, flags)) != CDG_OK) /* internal label */
		return result;
	if ((result = cdg_print_operand(gen, &tac->src1, CDG_PFLAG_APPEND_SPACE)) != CDG_OK) /* src1 */
		return result;

	tac_operand_t constant_literal = {
	    .kind = TAC_OPERAND_INTEGER, .type = SYMT_TYPE_INTEGER, .value.integer = 0};
	if ((result = cdg_print_operand(gen, &constant_literal, CDG_PFLAG_APPEND_LINE_FEED)) !=
	    CDG_OK) /* src2 (0) */
		return result;

	fprintf(gen->stream, "EXIT int@40\n");

	cdg_print_instr_direct(gen, "LABEL",
	                       CDG_PFLAG_APPEND_SPACE); /* internal label defined in the JUMPIFNEQ part */
	return cdg_print_label(gen, tac, CDG_LABEL_GENERIC,
	                       CDG_PFLAG_APPEND_LINE_FEED | CDG_PFLAG_INTERNAL_NAMESPACE);
}

/**
   @brief Generate a LABEL instruction. For param info, see @ref generator_func_t.
*/
cdg_result_t cdg_gen_label(codegen_t *gen, const tac_instruction_t *tac) {
	if (tac->opcode != TAC_LABEL)
		return CDG_INVALID_INSTRUCTION;

	/* General purpose label format: &# */
	fprintf(gen->stream, "LABEL &%lu\n", tac->label);
	return CDG_OK;
}

/**
   @brief Generate an unconditional JUMP instructio. For param info, see @ref generator_func_t.
*/
cdg_result_t cdg_gen_unconditional_jump(codegen_t *gen, const tac_instruction_t *tac) {
	if (tac->opcode != TAC_JUMP)
		return CDG_INVALID_INSTRUCTION;

	cdg_print_instr_direct(gen, "JUMP", CDG_PFLAG_APPEND_SPACE);
	return cdg_print_label(gen, tac, CDG_LABEL_GENERIC, CDG_PFLAG_APPEND_LINE_FEED);
}

/**
   @brief Generate a condition JUMP IF FALSE. For param info, see @ref generator_func_t.
*/
cdg_result_t cdg_gen_jump_if_false(codegen_t *gen, const tac_instruction_t *tac) {
	if (tac->opcode != TAC_JUMP_IF_FALSE)
		return CDG_INVALID_INSTRUCTION;
	cdg_result_t result = CDG_INVALID_INSTRUCTION;

	cdg_print_instr_direct(gen, "JUMPIFEQ", CDG_PFLAG_APPEND_SPACE);
	if ((result = cdg_print_label(gen, tac, CDG_LABEL_GENERIC, CDG_PFLAG_APPEND_SPACE)) != CDG_OK) /* label */
		return result;
	if ((result = cdg_print_operand(gen, &tac->src1, CDG_PFLAG_APPEND_SPACE)) != CDG_OK) /* src1 */
		return result;

	tac_operand_t conditional_literal = {
	    .kind = TAC_OPERAND_BOOLEAN, .type = SYMT_TYPE_BOOLEAN, .value.boolean = false};
	return cdg_print_operand(gen, &conditional_literal, CDG_PFLAG_APPEND_LINE_FEED); /* src2 (false) */
}

/**
   @brief Maps a TAC instruction to its respective code generator function.
   @note This relies on the definition order of tac_opcode_t. If it changes, this _must_ change with it.
*/
static const generator_func_t CODE_GEN_MAP[] = {
    cdg_gen_function_begin,  // [0]  TAC_FUNCTION_BEGIN
    cdg_gen_function_end,    // [1]  TAC_FUNCTION_END
    cdg_register_func_param, // [2]  TAC_PARAM

    cdg_gen_move,                 // [3]  TAC_MOVE
    cdg_gen_positive,             // [4]  TAC_POSITIVE
    cdg_gen_negative,             // [5]  TAC_NEGATIVE
    cdg_gen_not,                  // [6]  TAC_NOT
    cdg_gen_simple_binary,        // [7]  TAC_ADD
    cdg_gen_simple_binary,        // [8]  TAC_SUBTRACT
    cdg_gen_simple_binary,        // [9]  TAC_MULTIPLY
    cdg_gen_divide,               // [10] TAC_DIVIDE
    cdg_gen_simple_binary,        // [11] TAC_CONCATENATE
    cdg_gen_simple_binary,        // [12] TAC_LESS
    cdg_gen_nonstrict_inequality, // [13] TAC_LESS_EQUAL
    cdg_gen_simple_binary,        // [14] TAC_GREATER
    cdg_gen_nonstrict_inequality, // [15] TAC_GREATER_EQUAL
    cdg_gen_simple_binary,        // [16] TAC_EQUAL
    cdg_gen_not_equal,            // [17] TAC_NOT_EQUAL
    cdg_gen_simple_binary,        // [18] TAC_AND
    cdg_gen_simple_binary,        // [19] TAC_OR

    cdg_gen_func_arg,           // [20] TAC_ARG
    cdg_gen_func_call,          // [21] TAC_CALL
    cdg_gen_non_zero_test,      // [22] TAC_CHECK_NONZERO
    cdg_gen_func_return,        // [23] TAC_RETURN
    cdg_gen_label,              // [24] TAC_LABEL
    cdg_gen_unconditional_jump, // [25] TAC_JUMP
    cdg_gen_jump_if_false,      // [26] TAC_JUMP_IF_FALSE
};

#ifdef UNIT_UNDER_TEST
static const char *code_gen_names[] = {
    "cdg_gen_function_begin",
    "cdg_gen_function_end",
    "cdg_register_func_param",

    "cdg_gen_move",
    "cdg_gen_positive",
    "cdg_gen_negative",
    "cdg_gen_not",
    "cdg_gen_simple_binary",
    "cdg_gen_simple_binary",
    "cdg_gen_simple_binary",
    "cdg_gen_divide",
    "cdg_gen_simple_binary",
    "cdg_gen_simple_binary",
    "cdg_gen_nonstrict_inequality",
    "cdg_gen_simple_binary",
    "cdg_gen_nonstrict_inequality",
    "cdg_gen_simple_binary",
    "cdg_gen_not_equal",
    "cdg_gen_simple_binary",
    "cdg_gen_simple_binary",

    "cdg_gen_func_arg",
    "cdg_gen_func_call",
    "cdg_gen_non_zero_test",
    "cdg_gen_func_return",
    "cdg_gen_label",
    "cdg_gen_unconditional_jump",
    "cdg_gen_jump_if_false",
};
#endif

cdg_result_t codegen_generate_code(codegen_t *gen, const tac_t *code) {
	cdg_result_t result = CDG_OK;

	const tac_instruction_t *cur_instr = tac_first(code);
	while (cur_instr != NULL) {
		int code_gen_idx = (cur_instr->opcode - TAC_FUNCTION_BEGIN);
		if (code_gen_idx < 0 || code_gen_idx >= (int)sizeof(CODE_GEN_MAP)) {
			fprintf(stderr, "codegen_generate_code: code generator map index %d out of bounds [0, %lu]\n",
			        code_gen_idx, sizeof(CODE_GEN_MAP));
			return CDG_INTERNAL_FAILURE;
		}

		/* Get the code generator for the current instruction */
		generator_func_t cur_generator = CODE_GEN_MAP[code_gen_idx];

		/* Call the code gen on this instruction */
		if ((result = cur_generator(gen, cur_instr)) != CDG_OK) {
#ifdef UNIT_UNDER_TEST
			fprintf(stderr, "codegen_generate_code: code generator %s failed with code %d\n",
			        code_gen_names[code_gen_idx], result);
#else  /* UNIT_UNDER_TEST defined */
			fprintf(stderr, "codegen_generate_code: code generator @%p failed with code %d\n",
			        (void *)cur_generator, result);
#endif	/* UNIT_UNDER_TEST not defined */
			break;
		}

		/* If current is PARAM but next is not, print pending function parameters */
		if (cur_instr->opcode == TAC_PARAM && cur_instr->next != NULL &&
		    cur_instr->next->opcode != TAC_PARAM)
			if ((result = cdg_generate_func_params(gen)) != CDG_OK)
				break;

		cur_instr = cur_instr->next;
	}

	return result;
}
