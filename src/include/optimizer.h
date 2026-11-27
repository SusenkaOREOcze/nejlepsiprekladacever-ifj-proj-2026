/**
   @file optimizer.h
   @author xbosakd00 (DasBean125)
   @date 2026-11-07

   @brief Public interface for the three-address code optimizer module.
*/

#ifndef IFJ_OPTIMIZER_H
#define IFJ_OPTIMIZER_H

#include "hashtable.h"
#include "tac.h"

typedef enum OptResult {
	OPT_OK,
	OPT_BAD_ALLOC,
	OPT_BAD_ARG,
	OPT_INTERNAL_FAILURE,
	OPT_INVALID_INSTRUCTION,
	OPT_INVALID_LIST,
	OPT_REDEFINED_OPERAND_ID,
} opt_result_t;

typedef enum OptPropagationType {
	OPT_PROP_DYNAMIC, //!< Temporary propagates for instance the result of a non-const arithmetic op.
	OPT_PROP_LITERAL, //!< Temporary propagates a literal (constant) value.
	OPT_PROP_SYMBOL,  //!< Temporary propagates a symbol identifier.
	OPT_PROP_EXPR,    //!< Temporary propagates the result of a non-MOVE expression.
} opt_propag_type_t;

/**
   @brief Stores information about temporary variables (t#).
*/
typedef struct OptTempData {
	opt_propag_type_t propag_type;
	/**
	   Either a copy of the instruction in which this operand appeared (propag_type != OPT_PROP_EXPR), or
	   an instruction with only the .dst field defined which holds the result of an expression
	   (propag_type == OPT_PROP_EXPR).
	*/
	tac_instruction_t instr_data;
	/**
	   String version of the symbol's identifier (a 64-bit unsigned integer). The string length is a
	   multiple of 4, log_10(2^64-1) is 20 rounded up and not including the terminating \0 character,
	   hence 24 characters.
	*/
	char str_ident[24];
} opt_temp_data_t;

DEFTYPE_HASHTABLE(opt_temp_data_t) temp_data_lut_t; //!< Temporary variable data lookup table.

typedef struct Optimizer optimizer_t;

/**
   @brief Initializes the code optimizer state machine.

   @note Unless this function returns OPT_BAD_ARG or OPT_BAD_ALLOC, the original pointer is overwritten.

   @param opt An address of a pointer to the allocated optimizer.
   @returns OPT_OK on succes, OPT_BAD_ALLOC if allocation fails or OPT_BAD_ARG if the argument makes no sense.
*/
opt_result_t optimizer_init(optimizer_t **const opt);

/**
   @brief Free an optimizer from memory.

   If this function succeeds, the argument is set to NULL.

   @param opt A pointer to the variable that stores the optimizer.
   @returns OPT_OK on success or OPT_BAD_ARG if the arguments make no sense.
*/
opt_result_t optimizer_free(optimizer_t **opt);

/**
   @brief Perform the first optimization pass over a list of TACs.

   The first pass is a data gathering pass. Basically, this function will lock your list in a room, point
   reflectors at it, start a recorder, begin writing down everything it finds interesting and save its home
   address. Then it'll let it go home.

   @param opt The optimizer device. Must be initialized by @ref optimizer_init.
   @param tac The list of three-address codes to give a death stare to.
   @returns OPT_OK on success, OPT_BAD_ALLOC when memory takes a crap, OPT_INVALID_INSTRUCTION if a TAC
   instruction is semantically invalid, OPT_BAD_ARG if the arguments make no sense.
*/
opt_result_t optimizer_first_pass(optimizer_t *opt, tac_t *tac);

/**
   @brief Perform the second optimization pass over a list of TACs analyzed by @ref optimizer_first_pass.

   The second pass is the optimizing one. This pass will knock your list out in its house, wake it up in a
   room again, but instead of reflectors and recorders, there's a nanite bath that will turn some parts of
   your list into a sludge of raw data which is then metamorphosed into a part with similar, but more
   efficient function. We are not liable for any damages.

   @param opt The optimization machine.
   @param tac A list of TACs that's been passed over by @ref optimizer_first_pass. The nanomachines can't work
   on it if they aren't told what to do.
   @returns OPT_OK on success, OPT_BAD_ALLOC on allocation failure, OPT_BAD_ARG if the arguments don't make
   sense or OPT_INVALID_LIST if the passed list hasn't been analyzed by the first pass yet.
*/
opt_result_t optimizer_second_pass(optimizer_t *opt, tac_t *tac);

#endif /* optimizer.h included */
