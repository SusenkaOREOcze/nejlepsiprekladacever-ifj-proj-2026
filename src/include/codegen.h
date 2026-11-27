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

/*
  Reversed (bottom-up) base block generation algorithm:

  Move to last instruction (tac_last(list))
  While (instr is not NULL) Repeat
  If (instr is the last one || instr is a JUMP)
      If (pending(base block))
          Add instr->next to beginning of BB.
          Flush base block.
      
      Create new base block.
      Add instr to end of BB.
  Else If (instr is a LABEL)
      If (pending(base block))
          Add instr to beginning of BB.
          Flush base block.

      Create new base block.
      Add instr->prev to end of BB.
  End
*/


/*
  Identifier naming convention:
  Temporary variables:  t#		(e.g. LF@t3)
  Programmer variables: $#		(e.g. GF@$2)
  Flow control labels:	&#		(e.g. &6)
  Function labels:	_func_name	(e.g. _foo)

  [ TAC_FUNCTION_BEGIN ]
  * FUNCTION_BEGIN iterate
  > LABEL _iterate

  [ TAC_PARAM ]
  * PARAM s23
  > DEFVAR LF@$0	# make sure this doesn't interfere with base block variable definition determination
  > POPS LF@$0

  
  [ TAC_MOVE ]
  * t3 = MOVE ()
  > MOVE TF@t0 nil
  * t1 = MOVE 0
  > MOVE LF@t0 int@0
  * t60 = MOVE 1.0
  > MOVE TF@t0 float@0x1p0
  * s1 = MOVE "hello with specials"
  > MOVE LF@$1 string@hello\032with\032specials
  * s42 = MOVE true
  > MOVE GF@$42 bool@true

  [ TAC_NEGATIVE ]
  * t0 = TAC_NEGATIVE -7
  > MOVE LF@t0 int@7	# automatic literal conversion where possible
  * t5 = TAC_NEGATIVE t4
  > MUL LF@t1 LF@t0 {int@-1, float@-0x1p+0}	# constant depends on data type of t4
  
*/

#endif
