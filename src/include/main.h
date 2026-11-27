#ifndef IFJ_MAIN_H
#define IFJ_MAIN_H

/**
 * @file main.h
 * @author xkralil00 (SusenkaOREOcze)
 * @date 2026-11-26
 * @brief MAIN Compiler header file.
 */

typedef enum CompilerExitCode {
	COMPILER_OK = 0,
	COMPILER_LEXICAL_ERROR = 1,
	COMPILER_SYNTAX_ERROR = 2,
	COMPILER_UNDEFINED_ERROR = 3,
	COMPILER_REDEFINITION_ERROR = 4,
	COMPILER_ARGUMENT_ERROR = 5,
	COMPILER_TYPE_ERROR = 6,
	COMPILER_IMMUTABLE_ERROR = 7,
	COMPILER_SEMANTIC_ERROR = 10,
	COMPILER_INTERNAL_ERROR = 99
} compiler_exit_code_t;

#endif /* IFJ_MAIN_H */
