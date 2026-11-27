/**
   @file codegen.h
   @author xbosakd00 (DasBean125)
   @date 2026-11-23
   @brief Public interface for the target code (IFJcode26) generator module.
*/

#ifndef IFJ_CODEGEN_H
#define IFJ_CODEGEN_H

#include "tac.h"
#include "functable.h"
#include <stdio.h>

/*===============
  Data structures
  ===============*/

/**
   @brief Informs the user about how the result of a function call went.
*/
typedef enum CodegenResult {
	CDG_OK,
	CDG_BAD_ARG,
	CDG_BAD_ALLOC,
	CDG_INVALID_INSTRUCTION,
	CDG_INVALID_OPERAND,
	CDG_INTERNAL_FAILURE,
	CDG_NOT_A_SYMBOL,
} cdg_result_t;

/**
   @brief Stores the code generator state.
*/
typedef struct Codegen codegen_t;


/*=====================
  Function declarations
  =====================*/

/**
   @brief Allocates and initializes a code generator.

   @param[out] gen The address of a pointer to the newly created code generator.
   @param[in] func_table A reference to the function table used by previous translator stages.
   @returns CDG_OK on success, CDG_BAD_ALLOC on allocation failure or CDG_BAD_ARG if the arguments don't make
   sense.
*/
cdg_result_t codegen_init(codegen_t **gen, funct_t *func_table);

/**
   @brief Frees a previously code generator from memory.

   @note This function does _not_ free the function table reference passed to @ref codegen_init.
   @param[inout] The address of the pointer holding the generator's address. NULLed if this function succeeds.
   @returns CDG_OK on success or CDG_BAD_ARG if the arguments don't make sense.
*/
cdg_result_t codegen_free(codegen_t **gen);

/**
   @brief Runs the code generator.

   @param stream The stream to write the target code into. If NULL, defaults to 'stdout'.
   @param gen The codegen to run. No, not like that, it doesn't have legs, unless your autism makes it sound
   like the name of something that does... I'm sorry, what?
   @param[in] code The source three-address code array.
   @returns CDG_OK during a planetary parade, CDG_BAD_ARG if the arguments make no sense or CDG_BAD_ALLOC if
   somebody attempts using a sledgehammer on your DIMMs.
*/
cdg_result_t codegen_run(FILE *stream, codegen_t *gen, const tac_t *code);

#endif
