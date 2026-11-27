/**
   @file codegen_impl.h
   @author xbosakd00 (DasBean125)
   @date 2026-11-23
   @brief Private interface for the code generator module.

   Don't read the comments... t-they put something in them to-to make you autistic. I don't even remember why
   I wrote them.
*/

#ifndef IFJ_CODEGEN_IMPL_H
#define IFJ_CODEGEN_IMPL_H

#include "codegen.h"
#include "hashtable.h"

/*===========================
  Data structures & constants
  ===========================*/

/** @brief No special printing modifications. */
#define CDG_PFLAG_NONE (0x0)

/** @brief Operand is stored in the temporary frame. */
#define CDG_PFLAG_DECL_TEMP (0x1)

/** @brief Final output has a single space appended. */
#define CDG_PFLAG_APPEND_SPACE (0x2)

/** @brief Final output has a single line feed appended. */
#define CDG_PFLAG_APPEND_LINE_FEED (0x4)

/** @brief Operands are taken as internal (i.e. defined by the codegen, not by the irgen). */
#define CDG_PFLAG_INTERNAL_NAMESPACE (0x8)

/** @brief (LABEL only) Increments the internal label ID accumulator. */
#define CDG_PFLAG_NEW_LABEL (0x10)

#define CDG_LABEL_FUNC (true)
#define CDG_LABEL_GENERIC (false)

/**
   @brief Stores information about the address of a variable.

   An andress in this case is not a numerical offset in memory, but instead an identifier of that memory.
*/
typedef struct CdgAddress {
	tac_operand_kind_t type; //!< Symbol type (SYMBOL, TEMP etc.). Determines the identifier format.
	uint64_t code_id;        //!< Unique identifier received from the Codegen structure.
	bool is_local;           //!< If true, this symbol is in local (function) scope.
	bool is_defined;         //!< If true, this symbol has been defined by DEFVAR already.
} cdg_address_t;

/**
   @brief An associative table between a variable identifier and its address in memory.
*/
DEFTYPE_HASHTABLE(cdg_address_t) address_table_t;

/**
   @brief Stores the code generator state.
*/
struct Codegen {
	FILE *stream; //!< The output stream that will receive the generated code.

	bool in_local_scope;    //!< true if the current instruction is within a function's scope.
	uint32_t int_temp_acc;  //!< ID accumulator for internal temporary variables.
	uint32_t int_label_acc; //!< ID accumulator for internal labels.
	uint32_t loc_temp_acc;  //!< ID accumulator for local-scope temporary variables.
	uint32_t loc_sym_acc;   //!< ID accumulator for local-scope symbol variables.
	uint64_t temp_acc;      //!< ID accumulator for global-scope temporary variables.
	uint64_t sym_acc;       //!< ID accumulator for global-scope symbol variables.

	address_table_t addr_table;
	funct_t *func_table; //!< Reference to the functable used by previous translator stages.
	tac_operand_t *param_stack; //!< Parameter stack for proper PUSH/POP order determination.
	size_t pstack_size;
	size_t pstack_count;
    bool promote_arg; //!< If true, the argument must undergo numeric promotion.
	symt_data_type_t current_return_type;
};

/**
   @brief A function that converts a single TAC instruction to its corresponding IFJcode26 counterpart.
   @param 1 A reference to the code generator that called this function.
   @param 2 The instruction to convert.
   @returns CDG_OK if generation was successful, or any other error code. If this function returns an error,
   code generation fails with it.
*/
typedef cdg_result_t (*generator_func_t)(codegen_t *, const tac_instruction_t *);

/*=====================
  Function declarations
  =====================*/

/**
   @brief (Re)Allocate a parameter stack array in a code generator.

   The internal stack size field is updated if this function succeeds..
   @param gen The code generator whose parameter stack array to work on.
   @param size The desired array size.
   @returns CDG_OK on success, CDG_BAR_ARG with weird arguments or CDG_BAD_ALLOC on allocation failure.
*/
cdg_result_t cdg_alloc_param_stack(codegen_t *gen, const size_t size);

/**
   @brief Clears out the parameter stack of a code generator, without deallocating it.

   @param gen The code generator whose stack to vacuum.
   @returns CDG_OK on success or CDG_BAD_ARG if the argument is FUBAR.
*/
cdg_result_t cdg_clear_param_stack(codegen_t *gen);

/**
   @brief Append a parameter operand to the top of the parameter stack.

   @param gen The code generator whose stack to use.
   @param[in] op The parameter's operand.
   @returns CDG_OK on success, CDG_BAD_ARG when the arguments are invalid or CDG_BAD_ALLOC if the stack is not
   allocated.
*/
cdg_result_t cdg_append_param(codegen_t *gen, const tac_operand_t *op);

/**
   @brief Return the address of the given operand.

   If an address record does not yet exist, this function creates it first.
   @param gen The codegen to work with.
   @param dst The destination of the received address. This struct is always filled with data.
   @param op The operand whose address to retrieve.
   @param is_internal If true, then define temporary operands as internal, in their own namespace.
   @returns CDG_OK on success or when called with a non-symbol operand, CDG_BAD_ARG when invalid arguments are
   passed or CDG_BAD_ALLOC when symbol identifier name derivation or new record allocation fails.
*/
cdg_result_t cdg_get_symbol_address(codegen_t *gen, cdg_address_t *dst, const tac_operand_t *op,
                                    bool is_internal);

/**
   @brief Copy an operand from src to dst, converting it to a literal of the source type.

   @param dst The destination operand.
   @param src The source operand. Must be a symbol.
   @returns CDG_OK on success, CDG_BAD_ARG if the arguments are wrong or CDG_INVALID_OPERAND if src is not a
   symbol.
*/
cdg_result_t cdg_derive_literal_from_symbol(tac_operand_t *dst, const tac_operand_t *src);

/**
   @brief Print a literal operand in its equivalent IFJcode26 form.

   @param gen The codegen.
   @param op The operand to print.
   @returns CDG_OK on success, CDG_INTERNAL_FAILURE if the operand is not a literal or CDG_INVALID_OPERAND if
   the operand's data type is not defined.
*/
cdg_result_t cdg_print_literal(codegen_t *gen, const tac_operand_t *op);

/**
   @brief Convert a TAC operand to its equivalent IFJcode26 counterpart and print it.

   @note This function does _not_ terminate strings with a newline.
   @param gen The codegen It's back again and I don't know whvpů§ c
   @param op The operand to print.
   @param flags Modifies the output behavior using bit flags (combined with the | operator).
   See @ref CDG_PFLAG_NONE.
   @returns CDG_OK on success, CDG_INVALID_OPERAND if the operand is TAC_OPERAND_NONE or if it is a symbol
   when using CDG_PFLAG_ASSUME_INTERNAL, CDG_INTERNAL_FAILURE if the operand has no address record or
   CDG_BAD_ARG if the arguments make no sense.
*/
cdg_result_t cdg_print_operand(codegen_t *gen, const tac_operand_t *op, int flags);

/**
   @brief Print a variable definition from the provided operand.

   Prints conditionally - that is, only if the variable has not yet been defined.
   @param gen The codegen.
   @param op The operand whose definition to print.
   @param is_internal Specifies whether the operand should be defined from the internal namespace.
   @returns CDG_OK on success.
*/
cdg_result_t cdg_print_defvar(codegen_t *gen, const tac_operand_t *op, bool is_internal);

/**
   @brief Print a label name from an instruction.

   If the flag CDG_PFLAG_INTERNAL_NAMESPACE is used, prints an internal label ("&i#") with the value of the
   current label ID accumulator. To increment this accumulator before printing, use the flag
   CDG_PFLAG_NEW_LABEL.

   @param gen The codegen.
   @param instr The instruction to derive the label name from.
   @param is_func If true, the label is assumed to mark a function, otherwise it is a general purpose label.
   @param flags Modifies the output behavior using bit flags, just like @ref cdg_print_operand. This function
   only supports CDG_PFLAG_APPEND_SPACE/LINE_FEED, CDG_PFLAG_INTERNAL_NAMESPACE and CDG_PFLAG_NEW_LABEL.
   @returns CDG_OK on success or CDG_BAD_ARG when provided with invalid arguments.
*/
cdg_result_t cdg_print_label(codegen_t *gen, const tac_instruction_t *instr, bool is_func, int flags);

/**
   @brief Generate type conversion (promotion) code for binary instructions with source operands of type
   INT-DOUBLE/DOUBLE-IN.

   DOUBLE always has priority over INTEGER: the instruction `ADD GF\\@$0 int\\@4 float\\@0x1p0` will have the
   first source operand converted to a float equivalent `float\\@0x1p+2`.
   @param gen The code generator to print with.
   @param tac A binary instruction whose operands to promote.
*/
cdg_result_t cdg_prepare_numeric(codegen_t *gen, const tac_instruction_t *tac);

/**
   @brief Print a numeric operand's equivalent cast variable.

   @param gen The code generator.
   @param tac The instruction whose source operand cast variable to print.
   @param second Specifies which source operand to cast. If true, use src2, else use src1.
   @returns CDG_OK on success, or the return code of @ref cdg_print_operand.
*/
cdg_result_t cdg_print_numeric(codegen_t *gen, const tac_instruction_t *tac, bool second);

/**
   @brief Print an instruction directly from a string.

   @param gen The codegen.
   @param instr A string representing the instruction to print.
   @param flags Additional flags modifying the output. Supports only CDG_PFLAG_APPEND_SPACE/LINE_FEED.
*/
inline void cdg_print_instr_direct(const codegen_t *gen, const char *instr, int flags) {
	if (gen->stream == NULL)
		return;
	fprintf(gen->stream, "%s", instr);
	if (flags & CDG_PFLAG_APPEND_SPACE)
		fputc(' ', gen->stream);
	if (flags & CDG_PFLAG_APPEND_LINE_FEED)
		fputc('\n', gen->stream);
}

/**
   @brief Perform code generation from a code list.

   @param gen The codegen... it sees me.
   @param code The code list to generate from.
   @returns CDG_OK on successfull pass through the entire code list, CDG_BAD_ARG if the arguments are wrong,
   CDG_BAD_ALLOC on allocation failure or something else entirely.
*/
cdg_result_t codegen_generate_code(codegen_t *gen, const tac_t *code);

/* Extern inline declaration guard */
#ifdef IFJ_CODEGEN_EXTERN_PROVIDER

extern inline void cdg_print_instr_direct(const codegen_t *gen, const char *instr, int flags);

#endif /* IFJ_CODEGEN_EXTERN_PROVIDER defined */

#endif /* codegen_impl.h included */
