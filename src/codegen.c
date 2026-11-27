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

#include <inttypes.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

/**
   @brief Write IFJcode26 built-in functions to the output stream.

   Built-ins use the same stack argument order and LF\\@return convention as user functions.
   @param out The output stream to write to.
*/
static void cdg_emit_builtins(FILE *out) {
	static const char BUILTINS[] =
	    "LABEL _print\nDEFVAR LF@return\nDEFVAR LF@value\nDEFVAR LF@type\n"
	    "POPS LF@value\nTYPE LF@type LF@value\n"
	    "JUMPIFNEQ $print_value LF@type string@nil\nWRITE string@()\nJUMP $print_end\n"
	    "LABEL $print_value\nWRITE LF@value\nLABEL $print_end\n"
	    "MOVE LF@return nil@nil\nPOPFRAME\nRETURN\n"
	    "LABEL _readLine\nDEFVAR LF@return\nREAD LF@return string\n"
	    "JUMPIFNEQ $readLine_end LF@return nil@nil\nMOVE LF@return string@\n"
	    "LABEL $readLine_end\nPOPFRAME\nRETURN\n"
	    "LABEL _readInt\nDEFVAR LF@return\nDEFVAR LF@bad\nREAD LF@return int\n"
	    "JUMPIFEQ $readInt_invalid LF@return nil@nil\n"
	    "GT LF@bad LF@return int@2147483647\n"
	    "JUMPIFEQ $readInt_invalid LF@bad bool@true\n"
	    "LT LF@bad LF@return int@-2147483648\n"
	    "JUMPIFEQ $readInt_invalid LF@bad bool@true\n"
	    "JUMP $readInt_end\nLABEL $readInt_invalid\nMOVE LF@return int@0\n"
	    "LABEL $readInt_end\nPOPFRAME\nRETURN\n"
	    "LABEL _readDouble\nDEFVAR LF@return\nREAD LF@return float\n"
	    "JUMPIFNEQ $readDouble_end LF@return nil@nil\nMOVE LF@return float@0x0p+0\n"
	    "LABEL $readDouble_end\nPOPFRAME\nRETURN\n"
	    "LABEL _readBoolean\nDEFVAR LF@return\nREAD LF@return bool\n"
	    "JUMPIFNEQ $readBoolean_end LF@return nil@nil\nMOVE LF@return bool@false\n"
	    "LABEL $readBoolean_end\nPOPFRAME\nRETURN\n"
	    "LABEL _toInt\nDEFVAR LF@return\nDEFVAR LF@value\nDEFVAR LF@bad\nPOPS LF@value\n"
	    "GT LF@bad LF@value float@0x1.fffffffc00000p+30\n"
	    "JUMPIFEQ $toInt_max LF@bad bool@true\n"
	    "LT LF@bad LF@value float@-0x1p+31\n"
	    "JUMPIFEQ $toInt_min LF@bad bool@true\n"
	    "FLOAT2INT LF@return LF@value\nJUMP $toInt_end\n"
	    "LABEL $toInt_max\nMOVE LF@return int@2147483647\nJUMP $toInt_end\n"
	    "LABEL $toInt_min\nMOVE LF@return int@-2147483648\n"
	    "LABEL $toInt_end\nPOPFRAME\nRETURN\n"
	    "LABEL _strLength\nDEFVAR LF@return\nDEFVAR LF@value\nPOPS LF@value\n"
	    "STRLEN LF@return LF@value\nPOPFRAME\nRETURN\n"
	    "LABEL _chr\nDEFVAR LF@return\nDEFVAR LF@value\nPOPS LF@value\n"
	    "INT2CHAR LF@return LF@value\nPOPFRAME\nRETURN\n"
	    "LABEL _ord\nDEFVAR LF@return\nDEFVAR LF@value\nDEFVAR LF@index\n"
	    "DEFVAR LF@length\nDEFVAR LF@invalid\nPOPS LF@index\nPOPS LF@value\n"
	    "STRLEN LF@length LF@value\nLT LF@invalid LF@index int@0\n"
	    "JUMPIFEQ $ord_invalid LF@invalid bool@true\n"
	    "LT LF@invalid LF@index LF@length\n"
	    "JUMPIFEQ $ord_invalid LF@invalid bool@false\n"
	    "STRI2INT LF@return LF@value LF@index\nJUMP $ord_end\n"
	    "LABEL $ord_invalid\nMOVE LF@return int@-1\n"
	    "LABEL $ord_end\nPOPFRAME\nRETURN\n"
	    "LABEL _toStr\nDEFVAR LF@return\nDEFVAR LF@value\nDEFVAR LF@type\n"
	    "POPS LF@value\nTYPE LF@type LF@value\n"
	    "JUMPIFEQ $toStr_int LF@type string@int\n"
	    "JUMPIFEQ $toStr_float LF@type string@float\n"
	    "JUMPIFEQ $toStr_bool LF@type string@bool\n"
	    "JUMPIFEQ $toStr_nil LF@type string@nil\n"
	    "MOVE LF@return LF@value\nJUMP $toStr_end\n"
	    "LABEL $toStr_int\nINT2STR LF@return LF@value\nJUMP $toStr_end\n"
	    "LABEL $toStr_float\nFLOAT2STR LF@return LF@value\nJUMP $toStr_end\n"
	    "LABEL $toStr_bool\nMOVE LF@return string@false\n"
	    "JUMPIFEQ $toStr_end LF@value bool@false\n"
	    "MOVE LF@return string@true\nJUMP $toStr_end\n"
	    "LABEL $toStr_nil\nMOVE LF@return string@()\n"
	    "LABEL $toStr_end\nPOPFRAME\nRETURN\n"
	    "LABEL _subStr\nDEFVAR LF@return\nDEFVAR LF@value\nDEFVAR LF@start\n"
	    "DEFVAR LF@end\nDEFVAR LF@length\nDEFVAR LF@bad\nDEFVAR LF@char\n"
	    "POPS LF@end\nPOPS LF@start\nPOPS LF@value\n"
	    "STRLEN LF@length LF@value\nLT LF@bad LF@start int@0\n"
	    "JUMPIFEQ $subStr_error LF@bad bool@true\n"
	    "LT LF@bad LF@end int@0\nJUMPIFEQ $subStr_error LF@bad bool@true\n"
	    "GT LF@bad LF@start LF@end\nJUMPIFEQ $subStr_error LF@bad bool@true\n"
	    "LT LF@bad LF@start LF@length\nJUMPIFEQ $subStr_error LF@bad bool@false\n"
	    "GT LF@bad LF@end LF@length\nJUMPIFEQ $subStr_error LF@bad bool@true\n"
	    "MOVE LF@return string@\nLABEL $subStr_loop\n"
	    "JUMPIFEQ $subStr_done LF@start LF@end\n"
	    "GETCHAR LF@char LF@value LF@start\n"
	    "CONCAT LF@return LF@return LF@char\n"
	    "ADD LF@start LF@start int@1\nJUMP $subStr_loop\n"
	    "LABEL $subStr_error\nEXIT int@40\n"
	    "LABEL $subStr_done\nPOPFRAME\nRETURN\n"
	    "LABEL _floor\nDEFVAR LF@return\nDEFVAR LF@value\nDEFVAR LF@whole\n"
	    "DEFVAR LF@truncated\nDEFVAR LF@greater\nPOPS LF@value\n"
	    "MOVE LF@return LF@value\n"
	    "JUMPIFEQ $floor_end LF@value float@0x0p+0\n"
	    "GT LF@greater LF@value float@0x1p+52\n"
	    "JUMPIFEQ $floor_end LF@greater bool@true\n"
	    "LT LF@greater LF@value float@-0x1p+52\n"
	    "JUMPIFEQ $floor_end LF@greater bool@true\n"
	    "FLOAT2INT LF@whole LF@value\nINT2FLOAT LF@truncated LF@whole\n"
	    "GT LF@greater LF@truncated LF@value\n"
	    "JUMPIFEQ $floor_done LF@greater bool@false\n"
	    "SUB LF@whole LF@whole int@1\nLABEL $floor_done\n"
	    "INT2FLOAT LF@return LF@whole\nLABEL $floor_end\nPOPFRAME\nRETURN\n";
	fputs(BUILTINS, out);
}

/* IFJ26 Int wraps at 32 bits; IFJcode26 int arithmetic uses 64 bits. */
static cdg_result_t cdg_wrap_integer(codegen_t *gen, const tac_operand_t *dst) {
	if (gen == NULL || dst == NULL || gen->int_label_acc == UINT32_MAX)
		return CDG_BAD_ARG;
	const uint32_t label = ++gen->int_label_acc;
	FILE *out = gen->stream;
	fputs("IDIV LF@wrapq ", out);
	if (cdg_print_operand(gen, dst, CDG_PFLAG_APPEND_SPACE) != CDG_OK)
		return CDG_INVALID_OPERAND;
	fputs("int@4294967296\nMUL LF@wrapq LF@wrapq int@4294967296\nSUB ", out);
	if (cdg_print_operand(gen, dst, CDG_PFLAG_APPEND_SPACE) != CDG_OK ||
	    cdg_print_operand(gen, dst, CDG_PFLAG_APPEND_SPACE) != CDG_OK)
		return CDG_INVALID_OPERAND;
	fputs("LF@wrapq\nGT LF@wrapcmp ", out);
	if (cdg_print_operand(gen, dst, CDG_PFLAG_APPEND_SPACE) != CDG_OK)
		return CDG_INVALID_OPERAND;
	fprintf(out,
	        "int@2147483647\nJUMPIFEQ $wrap_high%" PRIu32 " LF@wrapcmp bool@true\n"
	        "LT LF@wrapcmp ",
	        label);
	if (cdg_print_operand(gen, dst, CDG_PFLAG_APPEND_SPACE) != CDG_OK)
		return CDG_INVALID_OPERAND;
	fprintf(out,
	        "int@-2147483648\nJUMPIFEQ $wrap_low%" PRIu32 " LF@wrapcmp bool@true\n"
	        "JUMP $wrap_end%" PRIu32 "\nLABEL $wrap_high%" PRIu32 "\nSUB ",
	        label, label, label);
	if (cdg_print_operand(gen, dst, CDG_PFLAG_APPEND_SPACE) != CDG_OK ||
	    cdg_print_operand(gen, dst, CDG_PFLAG_APPEND_SPACE) != CDG_OK)
		return CDG_INVALID_OPERAND;
	fprintf(out, "int@4294967296\nJUMP $wrap_end%" PRIu32 "\nLABEL $wrap_low%" PRIu32 "\nADD ", label, label);
	if (cdg_print_operand(gen, dst, CDG_PFLAG_APPEND_SPACE) != CDG_OK ||
	    cdg_print_operand(gen, dst, CDG_PFLAG_APPEND_SPACE) != CDG_OK)
		return CDG_INVALID_OPERAND;
	fprintf(out, "int@4294967296\nLABEL $wrap_end%" PRIu32 "\n", label);
	return ferror(out) ? CDG_INTERNAL_FAILURE : CDG_OK;
}

/**
   @brief Generate type conversion (promotion) code for binary instructions with source operands of type
   INT-DOUBLE/DOUBLE-IN.

   DOUBLE always has priority over INTEGER: the instruction `ADD GF\\@$0 int\\@4 float\\@0x1p0` will have the
   first source operand converted to a float equivalent `float\\@0x1p+2`.
   @param gen The code generator to print with.
   @param tac A binary instruction whose operands to promote.
*/
cdg_result_t cdg_prepare_numeric(codegen_t *gen, const tac_instruction_t *tac) {
	if (tac->src1.type == tac->src2.type)
		return CDG_OK;
	if (tac->src1.type == SYMT_TYPE_INTEGER && tac->src2.type == SYMT_TYPE_DOUBLE) {
		fputs("INT2FLOAT LF@cast1 ", gen->stream);
		return cdg_print_operand(gen, &tac->src1, CDG_PFLAG_APPEND_LINE_FEED);
	}
	if (tac->src2.type == SYMT_TYPE_INTEGER && tac->src1.type == SYMT_TYPE_DOUBLE) {
		fputs("INT2FLOAT LF@cast2 ", gen->stream);
		return cdg_print_operand(gen, &tac->src2, CDG_PFLAG_APPEND_LINE_FEED);
	}
	return CDG_OK;
}

/**
   @brief Print a numeric operand's equivalent cast variable.

   @param gen The code generator.
   @param tac The instruction whose source operand cast variable to print.
   @param second Specifies which source operand to cast. If true, use src2, else use src1.
   @returns CDG_OK on success, or the return code of @ref cdg_print_operand.
*/
cdg_result_t cdg_print_numeric(codegen_t *gen, const tac_instruction_t *tac, bool second) {
	const tac_operand_t *operand = second ? &tac->src2 : &tac->src1;
	if (operand->type == SYMT_TYPE_INTEGER &&
	    (second ? tac->src1.type : tac->src2.type) == SYMT_TYPE_DOUBLE) {
		fputs(second ? "LF@cast2" : "LF@cast1", gen->stream);
		fputc(second ? '\n' : ' ', gen->stream);
		return CDG_OK;
	}
	return cdg_print_operand(gen, operand, second ? CDG_PFLAG_APPEND_LINE_FEED : CDG_PFLAG_APPEND_SPACE);
}

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
	if (gen == NULL || *gen == NULL)
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
	cdg_result_t result = CDG_OK;

	/* Find a custom entrypoint (@main-annotated function that's not called "main") */
	const tac_instruction_t *entrypoint = NULL;
	for (const tac_instruction_t *instruction = tac_first(code); instruction != NULL;
	     instruction = instruction->next) {
		if (instruction->opcode == TAC_FUNCTION_BEGIN && instruction->is_entrypoint) {
			if (entrypoint != NULL || instruction->name == NULL)
				return CDG_INVALID_INSTRUCTION;
			entrypoint = instruction;
		}
	}

	/* This is mainly useful for testing */
	if (stream != NULL)
		gen->stream = stream;

	/* IFJcode26 header */
	cdg_print_instr_direct(gen, ".IFJcode26", CDG_PFLAG_APPEND_LINE_FEED);
	cdg_print_instr_direct(gen, "CREATEFRAME", CDG_PFLAG_APPEND_LINE_FEED);
	cdg_print_instr_direct(gen, "PUSHFRAME", CDG_PFLAG_APPEND_LINE_FEED);
	cdg_print_instr_direct(gen, "CALL", CDG_PFLAG_APPEND_SPACE);
	if (entrypoint != NULL) {
		if ((result = cdg_print_label(gen, entrypoint, CDG_LABEL_FUNC, CDG_PFLAG_APPEND_LINE_FEED)) != CDG_OK)
			return result;
	} else
		cdg_print_instr_direct(gen, "_main", CDG_PFLAG_APPEND_LINE_FEED);
	cdg_print_instr_direct(gen, "EXIT int@0", CDG_PFLAG_APPEND_LINE_FEED);
	cdg_emit_builtins(gen->stream);

	/* Run the machine of doom and despair */
	return codegen_generate_code(gen, code);
}

/*==================
  Internal functions
  ==================*/

cdg_result_t cdg_alloc_param_stack(codegen_t *gen, const size_t size) {
	if (gen == NULL || size == 0)
		return CDG_BAD_ARG;
	if (size > SIZE_MAX / sizeof(*gen->param_stack)) {
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
		if (gen->pstack_size > SIZE_MAX / 2)
			return CDG_BAD_ALLOC;
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
	if (src->type < SYMT_TYPE_UNIT || src->type >= SYMT_TYPE_UNDEFINED)
		return CDG_INVALID_OPERAND;

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
	if (gen == NULL || op == NULL)
		return CDG_BAD_ARG;
	if ((op->kind == TAC_OPERAND_INTEGER && op->type != SYMT_TYPE_INTEGER) ||
	    (op->kind == TAC_OPERAND_DOUBLE && op->type != SYMT_TYPE_DOUBLE) ||
	    (op->kind == TAC_OPERAND_STRING && (op->type != SYMT_TYPE_STRING || op->value.string == NULL)) ||
	    (op->kind == TAC_OPERAND_BOOLEAN && op->type != SYMT_TYPE_BOOLEAN) ||
	    (op->kind == TAC_OPERAND_UNIT && op->type != SYMT_TYPE_UNIT))
		return CDG_INVALID_OPERAND;
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
		if (instr->name == NULL)
			return CDG_INVALID_INSTRUCTION;
		const char *name = instr->name;
		if (strcmp(name, ".toString") == 0)
			name = "toStr";
		else if (strcmp(name, ".toInt") == 0)
			name = "toInt";
		else if (strcmp(name, ".length") == 0)
			name = "strLength";
		else if (strcmp(name, ".floor") == 0)
			name = "floor";
		fprintf(gen->stream, "_%s", name);
		goto PRINT_OK;
	}

	/* Normal label format: &#, internal label format: &i# */
	if (ass_internal) {
		gen->int_label_acc += (flags & CDG_PFLAG_NEW_LABEL) != 0;
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
		return CDG_INVALID_INSTRUCTION;
	gen->current_return_type = SYMT_TYPE_UNDEFINED;
	funct_symbol_t function;
	if (tac->name != NULL && funct_find_symbol(gen->func_table, tac->name, &function) == SYMTSTAT_OK)
		gen->current_return_type = function.ret_type;

	/* TAC symbol IDs may repeat in different functions. */
	HTBL_FREE(gen->addr_table);
	if (HTBL_INIT(gen->addr_table, 64) != HTBLSTAT_OK)
		return CDG_BAD_ALLOC;

	/* Function label */
	cdg_print_instr_direct(gen, "LABEL", CDG_PFLAG_APPEND_SPACE);
	if ((result = cdg_print_label(gen, tac, CDG_LABEL_FUNC, CDG_PFLAG_APPEND_LINE_FEED)) != CDG_OK)
		return result;

	/* Define (declare is a more correct term here) special function-scope variables.
	   These are used by type conversion, integer wrapping functions and by RETURN. */
	cdg_print_instr_direct(gen, "DEFVAR LF@return", CDG_PFLAG_APPEND_LINE_FEED);
	cdg_print_instr_direct(gen, "DEFVAR LF@wrapq", CDG_PFLAG_APPEND_LINE_FEED);
	cdg_print_instr_direct(gen, "DEFVAR LF@wrapcmp", CDG_PFLAG_APPEND_LINE_FEED);
	cdg_print_instr_direct(gen, "DEFVAR LF@cast1", CDG_PFLAG_APPEND_LINE_FEED);
	cdg_print_instr_direct(gen, "DEFVAR LF@cast2", CDG_PFLAG_APPEND_LINE_FEED);

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
	if (tac->dst.type == SYMT_TYPE_DOUBLE && tac->src1.type == SYMT_TYPE_INTEGER) {
		fputs("INT2FLOAT ", gen->stream);
		if ((result = cdg_print_operand(gen, &tac->dst, CDG_PFLAG_APPEND_SPACE)) != CDG_OK)
			return result;
		return cdg_print_operand(gen, &tac->src1, CDG_PFLAG_APPEND_LINE_FEED);
	}

	/* Print the symbolator */
	fprintf(gen->stream, "MOVE ");
	if ((result = cdg_print_operand(gen, &tac->dst, CDG_PFLAG_APPEND_SPACE)) != CDG_OK) /* dst */
		return result;
	result = cdg_print_operand(gen, &tac->src1, CDG_PFLAG_APPEND_LINE_FEED);

	/* Wrap result to 32bit integer range */
	if (result == CDG_OK && gen->in_local_scope && tac->dst.type == SYMT_TYPE_INTEGER &&
	    tac->src1.kind == TAC_OPERAND_INTEGER &&
	    (tac->src1.value.integer < INT32_MIN || tac->src1.value.integer > INT32_MAX))
		return cdg_wrap_integer(gen, &tac->dst);
	return result;
}

/**
   @brief Generate a POSITIVE instruction. For param info, see @ref generator_func_t.
*/
cdg_result_t cdg_gen_positive(codegen_t *gen, const tac_instruction_t *tac) {
	if (tac->opcode != TAC_POSITIVE)
		return CDG_INVALID_INSTRUCTION;
	cdg_result_t result = CDG_OK;

	/* Unary plus keeps the numeric value, whether it is a literal or a variable. */
	if ((tac->src1.type != SYMT_TYPE_INTEGER && tac->src1.type != SYMT_TYPE_DOUBLE) ||
	    (!tac_is_numeric_literal_operand(tac->src1.kind) && !tac_is_symbol_operand(tac->src1.kind)))
		return CDG_INVALID_OPERAND;
	if ((result = cdg_print_defvar(gen, &tac->dst, false)) != CDG_OK)
		return result;
	cdg_print_instr_direct(gen, "MOVE", CDG_PFLAG_APPEND_SPACE);
	if ((result = cdg_print_operand(gen, &tac->dst, CDG_PFLAG_APPEND_SPACE)) != CDG_OK) /* dst */
		return result;
	if ((result = cdg_print_operand(gen, &tac->src1, CDG_PFLAG_APPEND_LINE_FEED)) != CDG_OK) /* src */
		return result;

	/* Wrap result to 32bit integer range */
	if (result == CDG_OK && gen->in_local_scope && tac->dst.type == SYMT_TYPE_INTEGER &&
	    tac->src1.kind == TAC_OPERAND_INTEGER &&
	    (tac->src1.value.integer < INT32_MIN || tac->src1.value.integer > INT32_MAX))
		return cdg_wrap_integer(gen, &tac->dst);
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
		else {
			if (negative_operand.value.integer == INT64_MIN)
				return CDG_INVALID_OPERAND;
			negative_operand.value.integer = -negative_operand.value.integer;
		}
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

	/* Wrap result to 32bit integer range */
	if (result == CDG_OK && gen->in_local_scope && tac->dst.type == SYMT_TYPE_INTEGER)
		return cdg_wrap_integer(gen, &tac->dst);
	return result;
}

/**
   @brief Generate a NOT instruction. For param info, see @ref generator_func_t.
*/
cdg_result_t cdg_gen_not(codegen_t *gen, const tac_instruction_t *tac) {
	if (tac->opcode != TAC_NOT)
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
		instruction = tac->dst.type == SYMT_TYPE_STRING ? "CONCAT" : "ADD";
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
	const bool concatenate_strings = tac->opcode == TAC_ADD && tac->dst.type == SYMT_TYPE_STRING;
	if (!concatenate_strings && (result = cdg_prepare_numeric(gen, tac)) != CDG_OK)
		return result;

	cdg_print_instr_direct(gen, instruction, CDG_PFLAG_APPEND_SPACE);
	if ((result = cdg_print_operand(gen, &tac->dst, CDG_PFLAG_APPEND_SPACE)) != CDG_OK) /* dst */
		return result;
	if (concatenate_strings) {
		if ((result = cdg_print_operand(gen, &tac->src1, CDG_PFLAG_APPEND_SPACE)) != CDG_OK)
			return result;
		result = cdg_print_operand(gen, &tac->src2, CDG_PFLAG_APPEND_LINE_FEED);
	} else {
		if ((result = cdg_print_numeric(gen, tac, false)) != CDG_OK) /* left src */
			return result;
		result = cdg_print_numeric(gen, tac, true);
	}

	/* Wrap result to 32bit integer range */
	if (result == CDG_OK && gen->in_local_scope && tac->dst.type == SYMT_TYPE_INTEGER &&
	    (tac->opcode == TAC_ADD || tac->opcode == TAC_SUBTRACT || tac->opcode == TAC_MULTIPLY))
		return cdg_wrap_integer(gen, &tac->dst);
	return result;
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
	if (tac->src1.type == SYMT_TYPE_INTEGER && tac->src2.type == SYMT_TYPE_INTEGER)
		instruction = "IDIV";

	/* Print that s##t */
	if ((result = cdg_print_defvar(gen, &tac->dst, false)) != CDG_OK)
		return result;
	if ((result = cdg_prepare_numeric(gen, tac)) != CDG_OK)
		return result;

	cdg_print_instr_direct(gen, instruction, CDG_PFLAG_APPEND_SPACE);
	if ((result = cdg_print_operand(gen, &tac->dst, CDG_PFLAG_APPEND_SPACE)) != CDG_OK) /* dst */
		return result;
	if ((result = cdg_print_numeric(gen, tac, false)) != CDG_OK) /* left src */
		return result;
	result = cdg_print_numeric(gen, tac, true);

	/* Wrap result to 32bit integer range */
	if (result == CDG_OK && gen->in_local_scope && tac->dst.type == SYMT_TYPE_INTEGER)
		return cdg_wrap_integer(gen, &tac->dst);
	return result;
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
	char *main_instr = "GT"; /* a <= b is !(a > b) */
	if (tac->opcode == TAC_GREATER_EQUAL)
		main_instr = "LT"; /* a >= b is !(a < b) */

	if ((result = cdg_print_defvar(gen, &tac->dst, false)) != CDG_OK)
		return result;
	if ((result = cdg_prepare_numeric(gen, tac)) != CDG_OK)
		return result;

	cdg_print_instr_direct(gen, main_instr, CDG_PFLAG_APPEND_SPACE);
	if ((result = cdg_print_operand(gen, &tac->dst, CDG_PFLAG_APPEND_SPACE)) != CDG_OK)
		return result;
	if ((result = cdg_print_numeric(gen, tac, false)) != CDG_OK) /* left src */
		return result;
	if ((result = cdg_print_numeric(gen, tac, true)) != CDG_OK) /* right src */
		return result;

	/* NOT part */
	cdg_print_instr_direct(gen, "NOT", CDG_PFLAG_APPEND_SPACE);
	if ((result = cdg_print_operand(gen, &tac->dst, CDG_PFLAG_APPEND_SPACE)) != CDG_OK) /* dst */
		return result;
	return cdg_print_operand(gen, &tac->dst, CDG_PFLAG_APPEND_LINE_FEED);
}

/**
   @brief Generate a NOT EQUAL instruction.

   This is a series of instructions, because I just LOVE IFJcode26. For param info, see @ref generator_func_t.
*/
cdg_result_t cdg_gen_not_equal(codegen_t *gen, const tac_instruction_t *tac) {
	if (tac->opcode != TAC_NOT_EQUAL)
		return CDG_INVALID_INSTRUCTION;
	cdg_result_t result = CDG_INVALID_INSTRUCTION;

	if ((result = cdg_print_defvar(gen, &tac->dst, false)) != CDG_OK)
		return result;
	if ((result = cdg_prepare_numeric(gen, tac)) != CDG_OK)
		return result;

	/* EQ writes the destination, then NOT flips it in place. */
	cdg_print_instr_direct(gen, "EQ", CDG_PFLAG_APPEND_SPACE);
	if ((result = cdg_print_operand(gen, &tac->dst, CDG_PFLAG_APPEND_SPACE)) != CDG_OK)
		return result;
	if ((result = cdg_print_numeric(gen, tac, false)) != CDG_OK) /* left src */
		return result;
	if ((result = cdg_print_numeric(gen, tac, true)) != CDG_OK) /* right src */
		return result;

	/* NOT part */
	cdg_print_instr_direct(gen, "NOT", CDG_PFLAG_APPEND_SPACE);
	if ((result = cdg_print_operand(gen, &tac->dst, CDG_PFLAG_APPEND_SPACE)) != CDG_OK) /* dst */
		return result;
	return cdg_print_operand(gen, &tac->dst, CDG_PFLAG_APPEND_LINE_FEED);
}

/**
   @brief Generate a function argument push instruction.

   Arguments are passed through the stack and pushed _in order_ (i.e., left-to-right).
   For param info, see @ref generator_func_t.
*/
cdg_result_t cdg_gen_func_arg(codegen_t *gen, const tac_instruction_t *tac) {
	if (tac->opcode != TAC_ARG)
		return CDG_INVALID_INSTRUCTION;
	if (gen->promote_arg) {
		fputs("INT2FLOAT LF@cast1 ", gen->stream);
		cdg_result_t result = cdg_print_operand(gen, &tac->src1, CDG_PFLAG_APPEND_LINE_FEED);
		if (result != CDG_OK)
			return result;
		fputs("PUSHS LF@cast1\n", gen->stream);
		return CDG_OK;
	}

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

	if (tac->dst.kind != TAC_OPERAND_NONE) {
		if ((result = cdg_print_defvar(gen, &tac->dst, false)) != CDG_OK)
			return result;
		cdg_print_instr_direct(gen, "MOVE", CDG_PFLAG_APPEND_SPACE);
		if ((result = cdg_print_operand(gen, &tac->dst, CDG_PFLAG_APPEND_SPACE)) != CDG_OK)
			return result;
		cdg_print_instr_direct(gen, "TF@return", CDG_PFLAG_APPEND_LINE_FEED);
	}
	return result;
}

/**
   @brief Generate a function return instruction. For param info, see @ref generator_func_t.
*/
cdg_result_t cdg_gen_func_return(codegen_t *gen, const tac_instruction_t *tac) {
	if (tac->opcode != TAC_RETURN)
		return CDG_INVALID_INSTRUCTION;
	cdg_result_t result = CDG_INVALID_INSTRUCTION;

	/* Read local operands while LF is still active, then restore the caller frame. Also perform type
	   promotion from INT to DOUBLE if necessary. */
	char *instr = gen->current_return_type == SYMT_TYPE_DOUBLE && tac->src1.type == SYMT_TYPE_INTEGER
	                  ? "INT2FLOAT LF@return"
	                  : "MOVE LF@return";
	cdg_print_instr_direct(gen, instr, CDG_PFLAG_APPEND_SPACE);
	if ((result = cdg_print_operand(gen, &tac->src1, CDG_PFLAG_APPEND_LINE_FEED)) != CDG_OK)
		return result;
	cdg_print_instr_direct(gen, "POPFRAME", CDG_PFLAG_APPEND_LINE_FEED);

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
	int flags = CDG_PFLAG_APPEND_SPACE | CDG_PFLAG_INTERNAL_NAMESPACE | CDG_PFLAG_NEW_LABEL;
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

/**
   @brief Maps the identifier (the name) of every code generator to its TAC instruction index.
   This is equivalent to, yet very different from, @ref CODE_GEN_MAP.
*/
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

cdg_result_t codegen_generate_code(codegen_t *gen, const tac_t *code) {
	cdg_result_t result = CDG_OK;
	size_t arg_index = 0;

	const tac_instruction_t *cur_instr = tac_first(code);
	while (cur_instr != NULL) {
		/* Compute code generator function index */
		int code_gen_idx = (cur_instr->opcode - TAC_FUNCTION_BEGIN);
		if (code_gen_idx < 0 || code_gen_idx >= (int)(sizeof(CODE_GEN_MAP) / sizeof(CODE_GEN_MAP[0]))) {
			fprintf(stderr, "codegen_generate_code: code generator map index %d out of bounds [0, %lu]\n",
			        code_gen_idx, sizeof(CODE_GEN_MAP));
			return CDG_INTERNAL_FAILURE;
		}

		/* Conditionally promote function arguments by doing... something. Yeah idk */
		gen->promote_arg = false;
		if (cur_instr->opcode == TAC_ARG) {
			const tac_instruction_t *call = cur_instr->next;
			while (call != NULL && call->opcode == TAC_ARG)
				call = call->next;
			if (call == NULL || call->opcode != TAC_CALL || call->name == NULL)
				return CDG_INVALID_INSTRUCTION;
			symt_data_type_t expected = SYMT_TYPE_UNDEFINED;
			if (arg_index == 0 && (strcmp(call->name, "toInt") == 0 || strcmp(call->name, ".toInt") == 0 ||
			                       strcmp(call->name, ".floor") == 0))
				expected = SYMT_TYPE_DOUBLE;
			else {
				funct_symbol_t function;
				if (funct_find_symbol(gen->func_table, call->name, &function) == SYMTSTAT_OK &&
				    arg_index <= INT_MAX)
					(void)funct_nth_param_type(gen->func_table, &function, (int)arg_index, &expected);
			}
			gen->promote_arg = expected == SYMT_TYPE_DOUBLE && cur_instr->src1.type == SYMT_TYPE_INTEGER;
			arg_index++;
		} else
			arg_index = 0;

		/* Call the code generator for this instruction */
		generator_func_t cur_generator = CODE_GEN_MAP[code_gen_idx];
		if ((result = cur_generator(gen, cur_instr)) != CDG_OK) {
			fprintf(stderr, "codegen_generate_code: code generator %s failed with code %d\n",
			        code_gen_names[code_gen_idx], result);
			break;
		}

		/* Declare function locals before any branch can enter a loop body. */
		if (cur_instr->opcode == TAC_FUNCTION_BEGIN) {
			for (const tac_instruction_t *item = cur_instr->next;
			     item != NULL && item->opcode != TAC_FUNCTION_END; item = item->next) {
				if (tac_is_symbol_operand(item->dst.kind) &&
				    (result = cdg_print_defvar(gen, &item->dst, false)) != CDG_OK)
					return result;
			}
		}

		/* If current is PARAM but next is not, print pending function parameters */
		if (cur_instr->opcode == TAC_PARAM && cur_instr->next != NULL && cur_instr->next->opcode != TAC_PARAM)
			if ((result = cdg_generate_func_params(gen)) != CDG_OK)
				break;

		cur_instr = cur_instr->next;
	}

	return result;
}
