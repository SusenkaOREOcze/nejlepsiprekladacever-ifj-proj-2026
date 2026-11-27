/**
   @file optimizer_impl.h
   @author xbosakd00 (DasBean125)
   @date 2026-11-21

   @brief Private interface for the code optimizer module.
*/

#ifndef IFJ_OPTIMIZER_IMPL_H
#define IFJ_OPTIMIZER_IMPL_H

#include "optimizer.h"

/**
   @brief Stores the code optimizer state and data.
*/
struct Optimizer {
	temp_data_lut_t tmp_op_lut;
	tac_t *processed_list; //!< Address of a list that was processed by the first optimization pass.
};

/**
   @brief Determine the propagation type from the source operand of a MOVE opcode.

   @param opt The optimizer that stores the the operand data.
   @param dst Temp. operand record that will receive the propagation type. The .propag_type field is the only
   modified field by this function.
   @param src_instr The MOVE instruction to extract data from.
   @return OPT_OK on success, OPT_INVALID_INSTRUCTION if the encountered instruction is invalid or if the
   instruction is not a MOVE operation or OPT_BAD_ARG if the arguments are stupid.

   An invalid instruction is for example `t3 = MOVE t2` such that `t2` is not defined anywhere preceding it.
*/
opt_result_t opt_get_operand_propag_type(optimizer_t *opt, opt_temp_data_t *dst,
                                         const tac_instruction_t *src_instr);

/**
   @brief Scan the temporary operand contained in an instruction and save its data for further optimization.

   @note Should be called first during optimization.
   @param opt The optimizer that will store the data.
   @param instruction The instruction to scan. This should be an assignment instruction with a temporary
   destination.
   @returns OPT_OK on success or when the instruction is _not an assignment with a temporary destination_,
   OPT_INVALID_INSTRUCTION, OPT_BAD_ALLOC on allocation failure, OPT_BAD_ARG or OPT_REDEFINED_OPERAND_ID.
   In the last case, the lookup table is still updated with the redefined operand's data.
*/
opt_result_t opt_scan_temporary_operand(optimizer_t *opt, const tac_instruction_t *instruction);

/**
   @brief Delete a MOVE instruction that assigns to a redundant temporary variable.

   Candidate instructions for deletion are MOVE instructions with a non-dynamic propagation type, e.g.
   t1 = MOVE s42 [s42 is a symbol], t2 = MOVE 4 [4 is a constant literal].

   @note Should be called after @ref opt_scan_temporary_operand.
   @param opt The optimizer to work with.
   @param list The instruction list that contains the instruction.
   @param instr The instruction that may be deleted.
   @param[out] next The address of a pointer that will store the instruction which followed 'instr'.
   @returns OPT_OK on success or when 'instr' is not a deletion candidate (e.g. because it isn't a MOVE
   instruction), OPT_INVALID_INSTRUCTION if this instruction has no record in the look-up table (did you
   forget to run the first pass?), OPT_BAD_ARG if the arguments are goofy or OPT_INTERNAL_FAILURE.
*/
opt_result_t opt_delete_redundant_move(optimizer_t *opt, tac_t *list, const tac_instruction_t *instr,
                                       const tac_instruction_t **next);

/**
   @brief Replace any temporary operand(s) in an instruction with their propagated value.

   Any instruction which has at least one temporary source operand is a valid candidate. The temporary operand
   must propagate either a symbol, a literal or the result of a constant expression.

   @note Should be called after @ref opt_delete_redundant_move.
   @param opt The optimizer who (yes, he's alive) will optimizer.
   @param list The original instruction list.
   @param instr_pos Position of the instruction to optimize.
   @returns OPT_OK on success, OPT_BAD_ARG if the arguments make no sense or OPT_INTERNAL_FAILURE. Even if
   this function succeeds, no adjustment to the instruction may occur.
*/
opt_result_t opt_replace_temp_operands(optimizer_t *opt, tac_t *list, const tac_instruction_t *instr_pos);

/**
   @brief Evaluate a constant expression instruction with a temporary operand destination, then remove it.

   Any arithmetic instruction which has only constant (literal) source operands is a valid candidate. The
   instruction is removed because if its value can be determined at compile-time, it effectively acts as an
   assignment to a temporary variable and so is subject to the rules used by @ref opt_delete_redundant_move.

   @note Should be called after @ref opt_replace_temp_operands.
   @param opt The optimizer that will perform black magic on the code.
   @param list The list containing the instruction.
   @param instr_pos The instruction to optimize.
   @param[out] next Address of a pointer which will receive the instruction following the current one.
   @returns OPT_OK on success or when this function leaves the instruction unchanged, OPT_BAD_ARG if the
   arguments don't make sense, OPT_BAD_ALLOC on allocation failure, OPT_INTERNAL_FAILURE when something bad
   happens, or OPT_REDEFINED_OPERAND_ID. In the last case, the instruction is still removed.
*/
opt_result_t opt_evaluate_const_expression(optimizer_t *opt, tac_t *list, const tac_instruction_t *instr_pos,
                                           const tac_instruction_t **next);

/**
   @brief Perform a shallow copy of an instruction into a temporary operand data structure.

   Here, "shallow" means that only non-pointer data is copied. All pointers in the destination will be NULL.
   @param[out] dst_data The recipient of the copy.
   @param[in] src_instr The instruction to copy surface data from.

   This function does nothing with NULL arguments.
*/
void instruction_shallow_copy(opt_temp_data_t *dst_data, const tac_instruction_t *src_instr);

#endif /* optimizer_impl.h included */
