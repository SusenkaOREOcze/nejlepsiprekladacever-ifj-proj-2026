/**
   @file test_optimizer.c
   @author xbosakd00 (DasBean125)
   @date 2026-11-21
   @brief Tests the code optimizer module @ref "optimizer.c".
*/

#define UNIT_UNDER_TEST

#include "../include/optimizer_impl.h"
#include "../include/hashtable_impl.h"
#include "../include/codegen_shared.h"
#include "cmocka/cmocka.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#define EXEC_TEST_CATEGORY(name, tests)                                                                      \
	do {                                                                                                     \
		printf("\n[    " name "    ]\n");                                                                    \
		test_results |= (cmocka_run_group_tests_name((name), (tests), NULL, NULL));                          \
	} while (0)

static const tac_instruction_t EMPTY_INSTRUCTION = {0};

/**
   @brief Creates a new TAC instruction list with several different instructions, for more generalized tests.

   @param[out] dst An address of a pointer that will store the newly created list.
*/
static void create_mixed_instr_list(tac_t **dst) {
	assert_int_equal(tac_create(dst), TAC_OK);
	tac_instruction_t instr = {
	    //TODO: add like idk, fuckin 10 instructions or some shitx
	};

	assert_int_equal(tac_append(*dst, &instr), TAC_OK);
}

/*==========================
  Optimizer allocation tests
  ==========================*/
static void optimizer_init_returns(void **state) {
	(void)state;
	assert_int_equal(optimizer_init(NULL), OPT_BAD_ARG);
}

static void optimizer_init_wipes_previous(void **state) {
	(void)state;
	optimizer_t *opt = (void *)0xDEADBEEF;

	assert_int_equal(optimizer_init(&opt), OPT_OK);
	assert_ptr_not_equal(opt, (void *)0xDEADBEEF);
	assert_null(opt->processed_list);

	assert_int_equal(HTBL_FREE(opt->tmp_op_lut), HTBLSTAT_OK);
	free(opt);
}

static void optimizer_free_returns(void **state) {
	(void)state;
	assert_int_equal(optimizer_free(NULL), OPT_BAD_ARG);
}

static void optimizer_free_nulls_argument(void **state) {
	(void)state;
	optimizer_t *opt;
	assert_int_equal(optimizer_init(&opt), OPT_OK);

	assert_int_equal(optimizer_free(&opt), OPT_OK);
	assert_null(opt);
}

/*======================
  Optimizer helper tests
  ======================*/
static void make_operand_ident_returns(void **state) {
	(void)state;
	char str[20];
	tac_operand_t operand = {
	    .kind = TAC_OPERAND_NONE,
	    .type = SYMT_TYPE_UNDEFINED,
	};
	assert_false(make_operand_ident(NULL, 24, &operand, false));
	assert_false(make_operand_ident(str, 0, &operand, false));
	assert_false(make_operand_ident(str, sizeof(str), NULL, false));
}

static void make_operand_ident_max_value_terminates_string(void **state) {
	(void)state;
	char str[41] = "I am an ordinary toaster. What? SCP-426.";
	str[40] = ':'; /* dangerous */
	tac_operand_t operand = {
	    .kind = TAC_OPERAND_SYMBOL,
	    .type = SYMT_TYPE_UNDEFINED,
	    .value.id = ULONG_MAX,
	};

	assert_true(make_operand_ident(str, sizeof(str), &operand, false));
	bool terminated = false;
	for (int i = 0; i < 41; i++)
		if (str[i] == '\0')
			terminated = true;
	assert_true(terminated);
	assert_string_equal(str, "18446744073709551615"); /* 20 chars long */
}

static void make_operand_ident_improper_size_no_overflow(void **state) {
	(void)state;
	char str[12] = "12345678901";
	str[11] = ':'; /* Shake hands with danger */
	tac_operand_t operand = {
	    .kind = TAC_OPERAND_SYMBOL,
	    .type = SYMT_TYPE_UNDEFINED,
	    .value.id = ULONG_MAX,
	};

	/* Should return false but still save part of the string */
	assert_false(make_operand_ident(str, sizeof(str), &operand, false));
	bool terminated = false;
	for (int i = 0; i < 12; i++)
		if (str[i] == '\0')
			terminated = true;
	assert_true(terminated);
	assert_string_equal(str, "18446744073"); /* 11 chars long (not including terminator) */
}

/**
   @brief Tests that passing a char array exactly long enough to fit the highest number still passes.
*/
static void make_operand_ident_size_matches_str_len(void **state) {
	(void)state;
	char str[21] = "He had a lot to say.";
	str[20] = ':';
	tac_operand_t operand = {
	    .kind = TAC_OPERAND_SYMBOL,
	    .type = SYMT_TYPE_UNDEFINED,
	    .value.id = ULONG_MAX,
	};

	assert_true(make_operand_ident(str, sizeof(str), &operand, false));
	assert_int_equal(str[20], '\0');
}

static void instr_shallow_copy_full_bore_test(void **state) {
	(void)state;
	tac_instruction_t instr = {
	    .dst = {.kind = TAC_OPERAND_TEMP, .value.id = 42},
	    .src1 = {.kind = TAC_OPERAND_INTEGER, .value.integer = 3},
	    .src2 = {.kind = TAC_OPERAND_STRING, .value.string = "hello"},
	    .next = (void *)0xDEADBEEF,
	    .prev = (void *)0xDEADCEEF,
	    .name = "I shouldn't exist",
	    .label = 69,
	    .opcode = TAC_DIVIDE,
	};
	opt_temp_data_t copy = {
	    .instr_data = EMPTY_INSTRUCTION,
	    .propag_type = OPT_PROP_DYNAMIC,
	    .str_ident = "nuh-uh",
	};

	instruction_shallow_copy(NULL, &instr);
	instruction_shallow_copy(&copy, NULL); /* shouldn't change anything */
	assert_memory_equal(&copy.instr_data, &EMPTY_INSTRUCTION, sizeof(EMPTY_INSTRUCTION));

	instruction_shallow_copy(&copy, &instr); /* definitely changes something */
	assert_string_equal(copy.str_ident, "nuh-uh");
	assert_int_equal(copy.propag_type, OPT_PROP_DYNAMIC);

	/* Checking the instruction structure element-by-element, cause we wanna make sure */
	assert_null(copy.instr_data.prev);
	assert_null(copy.instr_data.next);
	assert_null(copy.instr_data.name);

	assert_uint_equal(copy.instr_data.label, 69);
	assert_int_equal(copy.instr_data.opcode, TAC_DIVIDE);
	assert_int_equal(copy.instr_data.dst.kind, TAC_OPERAND_TEMP);
	assert_uint_equal(copy.instr_data.dst.value.id, 42);
	assert_int_equal(copy.instr_data.src1.kind, TAC_OPERAND_INTEGER);
	assert_int_equal(copy.instr_data.src1.value.integer, 3);
	assert_int_equal(copy.instr_data.src2.kind, TAC_OPERAND_STRING);
	assert_string_equal(copy.instr_data.src2.value.string, "hello");
}

static void opt_is_assignment_opcode_valid_types(void **state) {
	(void)state;
	assert_true(tac_is_assignment_opcode(TAC_MOVE));
	assert_true(tac_is_assignment_opcode(TAC_POSITIVE));
	assert_true(tac_is_assignment_opcode(TAC_NEGATIVE));
	assert_true(tac_is_assignment_opcode(TAC_NOT));
	assert_true(tac_is_assignment_opcode(TAC_ADD));
	assert_true(tac_is_assignment_opcode(TAC_SUBTRACT));
	assert_true(tac_is_assignment_opcode(TAC_MULTIPLY));
	assert_true(tac_is_assignment_opcode(TAC_DIVIDE));
	assert_true(tac_is_assignment_opcode(TAC_CONCATENATE));
	assert_true(tac_is_assignment_opcode(TAC_LESS));
	assert_true(tac_is_assignment_opcode(TAC_LESS_EQUAL));
	assert_true(tac_is_assignment_opcode(TAC_GREATER_EQUAL));
	assert_true(tac_is_assignment_opcode(TAC_GREATER));
	assert_true(tac_is_assignment_opcode(TAC_EQUAL));
	assert_true(tac_is_assignment_opcode(TAC_NOT_EQUAL));
	assert_true(tac_is_assignment_opcode(TAC_AND));
	assert_true(tac_is_assignment_opcode(TAC_OR));
}

static void opt_is_assignment_opcode_invalid_types(void **state) {
	(void)state;
	assert_false(tac_is_assignment_opcode(TAC_FUNCTION_BEGIN));
	assert_false(tac_is_assignment_opcode(TAC_FUNCTION_END));
	assert_false(tac_is_assignment_opcode(TAC_PARAM));
	assert_false(tac_is_assignment_opcode(TAC_ARG));
	assert_false(tac_is_assignment_opcode(TAC_CALL));
	assert_false(tac_is_assignment_opcode(TAC_CHECK_NONZERO));
	assert_false(tac_is_assignment_opcode(TAC_RETURN));
	assert_false(tac_is_assignment_opcode(TAC_LABEL));
	assert_false(tac_is_assignment_opcode(TAC_JUMP));
	assert_false(tac_is_assignment_opcode(TAC_JUMP_IF_FALSE));
}

static void opt_is_literal_operand_valid_types(void **state) {
	(void)state;
	assert_true(tac_is_literal_operand(TAC_OPERAND_INTEGER));
	assert_true(tac_is_literal_operand(TAC_OPERAND_DOUBLE));
	assert_true(tac_is_literal_operand(TAC_OPERAND_BOOLEAN));
	assert_true(tac_is_literal_operand(TAC_OPERAND_STRING));
	assert_true(tac_is_literal_operand(TAC_OPERAND_UNIT));
}

static void opt_is_literal_operand_invalid_types(void **state) {
	(void)state;
	assert_false(tac_is_literal_operand(TAC_OPERAND_TEMP));
	assert_false(tac_is_literal_operand(TAC_OPERAND_NONE));
	assert_false(tac_is_literal_operand(TAC_OPERAND_SYMBOL));
}

/*==========================
  Optimizer propagation type
  ==========================*/
static void get_operand_propagation_type_returns(void **state) {
	(void)state;
	opt_temp_data_t data;
	optimizer_t *opt = (void *)0xEADDDD23;
	tac_instruction_t src = {
	    /* TAC: FUNCTION_BEGIN "perkele" */
	    .label = 0,
	    .name = "perkele",
	    .next = NULL,
	    .prev = NULL,
	    .opcode = TAC_FUNCTION_BEGIN,
	    .dst = {.kind = TAC_OPERAND_TEMP, .value.id = 23},
	    .src1 = {.kind = TAC_OPERAND_SYMBOL, .value.id = 42},
	    .src2 = {.kind = TAC_OPERAND_NONE},
	};

	assert_int_equal(opt_get_operand_propag_type(NULL, &data, &src), OPT_BAD_ARG);
	assert_int_equal(opt_get_operand_propag_type(opt, NULL, &src), OPT_BAD_ARG);
	assert_int_equal(opt_get_operand_propag_type(opt, &data, NULL), OPT_BAD_ARG);
	assert_int_equal(opt_get_operand_propag_type(opt, &data, &src), OPT_INVALID_INSTRUCTION);
}

static void get_operand_propagation_type_single_literal(void **state) {
	(void)state;
	opt_temp_data_t out_data = {
	    .instr_data = {0},
	    .propag_type = OPT_PROP_DYNAMIC,
	    .str_ident = "",
	};
	optimizer_t *opt = (void *)0xDEADBEEF42; /* slightly dangerous, but we won't be using the LUT here */
	tac_instruction_t src = {
	    /* TAC: t23 = MOVE 69 */
	    .label = 0,
	    .name = "",
	    .next = NULL,
	    .prev = NULL,
	    .opcode = TAC_MOVE,
	    .dst = {.kind = TAC_OPERAND_TEMP, .value.id = 23},
	    .src1 = {.kind = TAC_OPERAND_INTEGER, .value.integer = 69},
	    .src2 = {.kind = TAC_OPERAND_NONE},
	};

	assert_int_equal(opt_get_operand_propag_type(opt, &out_data, &src), OPT_OK);
	assert_int_equal(out_data.propag_type, OPT_PROP_LITERAL);
	assert_string_equal(out_data.str_ident, "");
	assert_memory_equal(&EMPTY_INSTRUCTION, &out_data.instr_data, sizeof(EMPTY_INSTRUCTION));
}

static void get_operand_propagation_type_all_literals(void **state) {
	(void)state;
	opt_temp_data_t out_data = {
	    .instr_data = {0},
	    .propag_type = OPT_PROP_DYNAMIC,
	    .str_ident = "",
	};
	optimizer_t *opt = (void *)0xDEADBEEF42;
	tac_instruction_t src = {
	    .label = 0,
	    .name = "",
	    .next = NULL,
	    .prev = NULL,
	    .opcode = TAC_MOVE,
	    .dst = {.kind = TAC_OPERAND_TEMP, .value.id = 23},
	    .src1 = {.kind = TAC_OPERAND_NONE},
	    .src2 = {.kind = TAC_OPERAND_NONE},
	};

	/* t23 = MOVE 420 */ src.src1.kind = TAC_OPERAND_INTEGER;
	src.src1.value.integer = 420;
	assert_int_equal(opt_get_operand_propag_type(opt, &out_data, &src), OPT_OK);
	assert_int_equal(out_data.propag_type, OPT_PROP_LITERAL);

	/* t23 = MOVE 4.21 */
	src.src1.kind = TAC_OPERAND_DOUBLE;
	src.src1.value.decimal = 4.21;
	assert_int_equal(opt_get_operand_propag_type(opt, &out_data, &src), OPT_OK);
	assert_int_equal(out_data.propag_type, OPT_PROP_LITERAL);

	/* t23 = MOVE "Hi" */ src.src1.kind = TAC_OPERAND_STRING;
	src.src1.value.string = "Hi";
	assert_int_equal(opt_get_operand_propag_type(opt, &out_data, &src), OPT_OK);
	assert_int_equal(out_data.propag_type, OPT_PROP_LITERAL);

	/* t23 = MOVE true */ src.src1.kind = TAC_OPERAND_BOOLEAN;
	src.src1.value.boolean = true;
	assert_int_equal(opt_get_operand_propag_type(opt, &out_data, &src), OPT_OK);
	assert_int_equal(out_data.propag_type, OPT_PROP_LITERAL);

	/* t23 = MOVE nil */ src.src1.kind = TAC_OPERAND_UNIT;
	src.src1.value.integer = 0;
	assert_int_equal(opt_get_operand_propag_type(opt, &out_data, &src), OPT_OK);
	assert_int_equal(out_data.propag_type, OPT_PROP_LITERAL);
}

static void get_operand_propagation_type_symbol(void **state) {
	(void)state;
	opt_temp_data_t out_data = {
	    .instr_data = {0},
	    .propag_type = OPT_PROP_DYNAMIC,
	    .str_ident = "",
	};
	optimizer_t *opt = (void *)0xDEADBEEF42;
	tac_instruction_t src = {
	    /* TAC: t23 = MOVE s42 */
	    .label = 0,
	    .name = "",
	    .next = NULL,
	    .prev = NULL,
	    .opcode = TAC_MOVE,
	    .dst = {.kind = TAC_OPERAND_TEMP, .value.id = 23},
	    .src1 = {.kind = TAC_OPERAND_SYMBOL, .value.id = 42}, /* id is irrelevant here, but whatever */
	    .src2 = {.kind = TAC_OPERAND_NONE},
	};

	assert_int_equal(opt_get_operand_propag_type(opt, &out_data, &src), OPT_OK);
	assert_int_equal(out_data.propag_type, OPT_PROP_SYMBOL);
	assert_string_equal(out_data.str_ident, "");
	assert_memory_equal(&out_data.instr_data, &EMPTY_INSTRUCTION, sizeof(EMPTY_INSTRUCTION));
}

static void get_operand_propagation_type_none(void **state) {
	(void)state;
	opt_temp_data_t out_data = {
	    .instr_data = {0},
	    .propag_type = OPT_PROP_DYNAMIC,
	    .str_ident = "",
	};
	optimizer_t *opt = (void *)0xDEADBEEF42;
	tac_instruction_t src = {
	    /* TAC: t23 = MOVE (none) */
	    .label = 0,
	    .name = "",
	    .next = NULL,
	    .prev = NULL,
	    .opcode = TAC_MOVE,
	    .dst = {.kind = TAC_OPERAND_TEMP, .value.id = 23},
	    .src1 = {.kind = TAC_OPERAND_NONE},
	    .src2 = {.kind = TAC_OPERAND_NONE},
	};

	/* Does nothing if the src operand is of type TAC_OPERAND_NONE */
	assert_int_equal(opt_get_operand_propag_type(opt, &out_data, &src), OPT_OK);
	assert_int_equal(out_data.propag_type, OPT_PROP_DYNAMIC);
	assert_string_equal(out_data.str_ident, "");
	assert_memory_equal(&out_data.instr_data, &EMPTY_INSTRUCTION, sizeof(EMPTY_INSTRUCTION));
}

static void get_operand_propagation_type_undefined_temp_src(void **state) {
	(void)state;
	opt_temp_data_t out_data = {
	    .instr_data = {0},
	    .propag_type = OPT_PROP_DYNAMIC,
	    .str_ident = "",
	};
	optimizer_t *opt;
	tac_instruction_t src = {
	    /* TAC: t23 = MOVE t41 */
	    .label = 0,
	    .name = "",
	    .next = NULL,
	    .prev = NULL,
	    .opcode = TAC_MOVE,
	    .dst = {.kind = TAC_OPERAND_TEMP, .value.id = 23},
	    .src1 = {.kind = TAC_OPERAND_TEMP, .value.id = 41},
	    .src2 = {.kind = TAC_OPERAND_NONE},
	};

	assert_int_equal(optimizer_init(&opt), OPT_OK);
	opt_temp_data_t src_data = {
	    /* Adding t42 (not what we'll be trying to read!) */
	    .propag_type = OPT_PROP_DYNAMIC,
	    .str_ident = "42",
	};
	assert_int_equal(HTBL_INSERT(opt->tmp_op_lut, "42", &src_data), HTBLSTAT_OK);
	assert_int_equal(opt_get_operand_propag_type(opt, &out_data, &src), OPT_INVALID_INSTRUCTION);
	assert_int_equal(optimizer_free(&opt), OPT_OK);
}

static void get_operand_propagation_type_defined_temp_src(void **state) {
	(void)state;
	opt_temp_data_t out_data = {
	    .instr_data = {0},
	    .propag_type = OPT_PROP_DYNAMIC,
	    .str_ident = "",
	};
	optimizer_t *opt;
	tac_instruction_t src = {
	    /* TAC: t23 = MOVE t41 */
	    .label = 0,
	    .name = "",
	    .next = NULL,
	    .prev = NULL,
	    .opcode = TAC_MOVE,
	    .dst = {.kind = TAC_OPERAND_TEMP, .value.id = 23},
	    .src1 = {.kind = TAC_OPERAND_TEMP, .value.id = 42},
	    .src2 = {.kind = TAC_OPERAND_NONE},
	};

	assert_int_equal(optimizer_init(&opt), OPT_OK);
	opt_temp_data_t src_data = {
	    /* Adding t42, which is what we'll read this time */
	    .propag_type = OPT_PROP_LITERAL,
	    .str_ident = "42",
	};
	assert_int_equal(HTBL_INSERT(opt->tmp_op_lut, "42", &src_data), HTBLSTAT_OK);
	assert_int_equal(opt_get_operand_propag_type(opt, &out_data, &src), OPT_OK);
	assert_int_equal(out_data.propag_type, OPT_PROP_LITERAL);
	assert_memory_equal(&out_data.instr_data, &EMPTY_INSTRUCTION, sizeof(EMPTY_INSTRUCTION));
	assert_int_equal(optimizer_free(&opt), OPT_OK);
}

/*========
  Scanners
  ========*/
static void scan_temporary_operand_skip(void **state) {
	(void)state;
	optimizer_t *opt = (void *)0x4444422222;
	tac_instruction_t instr = {
	    .label = 0,
	    .name = "",
	    .next = NULL,
	    .prev = NULL,
	    .opcode = TAC_JUMP_IF_FALSE,
	    .dst = {.kind = TAC_OPERAND_TEMP, .value.id = 12},
	};

	/* if it does something, it'll caused a segmentation fault because of the random address of opt */
	assert_int_equal(opt_scan_temporary_operand(opt, &instr), OPT_OK);
	instr.dst.kind = TAC_OPERAND_NONE;
	instr.opcode = TAC_AND;
	assert_int_equal(opt_scan_temporary_operand(opt, &instr), OPT_OK);
}

static void scan_temporary_operand_single_non_move(void **state) {
	(void)state;
	optimizer_t *opt;
	assert_int_equal(optimizer_init(&opt), OPT_OK);
	tac_instruction_t instr = {
	    .opcode = TAC_SUBTRACT, /* not a MOVE operation, so it won't be deriving propagation */
	    .dst = {.kind = TAC_OPERAND_TEMP, .value.id = 125},
	    .src1 = {.kind = TAC_OPERAND_INTEGER, .value.integer = 20},
	};
	opt_temp_data_t out_data;

	assert_false(HTBL_EXISTS(opt->tmp_op_lut, "125"));
	assert_int_equal(opt_scan_temporary_operand(opt, &instr), OPT_OK);
	assert_true(HTBL_RETRIEVE(opt->tmp_op_lut, "125", out_data));

	assert_int_equal(out_data.propag_type, OPT_PROP_DYNAMIC);
	assert_string_equal(out_data.str_ident, "125");
	assert_int_equal(optimizer_free(&opt), OPT_OK);
}

static void scan_temporary_operand_single_move(void **state) {
	(void)state;
	optimizer_t *opt;
	assert_int_equal(optimizer_init(&opt), OPT_OK);
	tac_instruction_t instr = {
	    .opcode = TAC_MOVE,
	    .dst = {.kind = TAC_OPERAND_TEMP, .value.id = 121},
	    .src1 = {.kind = TAC_OPERAND_INTEGER, .value.integer = 20},
	};
	opt_temp_data_t out_data;

	assert_false(HTBL_EXISTS(opt->tmp_op_lut, "121"));
	assert_int_equal(opt_scan_temporary_operand(opt, &instr), OPT_OK);
	assert_true(HTBL_RETRIEVE(opt->tmp_op_lut, "121", out_data));

	assert_int_equal(out_data.propag_type, OPT_PROP_LITERAL);
	assert_string_equal(out_data.str_ident, "121");
	assert_int_equal(optimizer_free(&opt), OPT_OK);
}

static void scan_temporary_operand_redefinition(void **state) {
	(void)state;
	optimizer_t *opt;
	assert_int_equal(optimizer_init(&opt), OPT_OK);
	tac_instruction_t instr = {
	    .opcode = TAC_MOVE,
	    .dst = {.kind = TAC_OPERAND_TEMP, .value.id = 11},
	    .src1 = {.kind = TAC_OPERAND_INTEGER, .value.integer = 20},
	};
	opt_temp_data_t opr_data = {
	    .str_ident = "11",
	    .propag_type = OPT_PROP_SYMBOL,
	};

	assert_int_equal(HTBL_INSERT(opt->tmp_op_lut, "11", &opr_data), HTBLSTAT_OK);
	assert_int_equal(opt_scan_temporary_operand(opt, &instr), OPT_REDEFINED_OPERAND_ID);

	assert_true(HTBL_RETRIEVE(opt->tmp_op_lut, "11", opr_data));
	assert_int_equal(optimizer_free(&opt), OPT_OK);
}

/*=======================
  First optimization pass
  =======================*/
static void optimizer_first_pass_empty_list(void **state) {
	(void)state;
	optimizer_t *opt;
	tac_t *list;
	assert_int_equal(optimizer_init(&opt), OPT_OK);
	assert_int_equal(tac_create(&list), TAC_OK);

	assert_int_equal(optimizer_first_pass(NULL, list), OPT_BAD_ARG);
	assert_int_equal(optimizer_first_pass(opt, NULL), OPT_BAD_ARG);
	assert_int_equal(optimizer_first_pass(opt, list), OPT_OK);
	assert_ptr_equal(opt->processed_list, list);

	assert_int_equal(optimizer_free(&opt), OPT_OK);
	tac_destroy(&list);
}

static void optimizer_first_pass_single_instruction(void **state) {
	(void)state;
	optimizer_t *opt;
	tac_t *list;
	tac_instruction_t instr = {
	    .opcode = TAC_MOVE,
	    .dst = {.kind = TAC_OPERAND_TEMP, .value.id = 25},
	    .src1 = {.kind = TAC_OPERAND_INTEGER, .value.integer = 690},
	};
	assert_int_equal(optimizer_init(&opt), OPT_OK);
	assert_int_equal(tac_create(&list), TAC_OK);
	assert_int_equal(tac_append(list, &instr), TAC_OK);

	assert_int_equal(optimizer_first_pass(opt, list), OPT_OK);
	opt_temp_data_t out_data;
	assert_true(HTBL_RETRIEVE(opt->tmp_op_lut, "25", out_data));
	assert_string_equal(out_data.str_ident, "25");
	assert_int_equal(out_data.propag_type, OPT_PROP_LITERAL);

	assert_int_equal(optimizer_free(&opt), OPT_OK);
	tac_destroy(&list);
}

static void optimizer_first_pass_infers_propagation(void **state) {
	(void)state;
	optimizer_t *opt;
	tac_t *list;
	tac_instruction_t instr = {
	    .opcode = TAC_MOVE,
	    .dst = {.kind = TAC_OPERAND_TEMP, .value.id = 12},
	    .src1 = {.kind = TAC_OPERAND_DOUBLE, .value.decimal = 4.2},
	};
	assert_int_equal(optimizer_init(&opt), OPT_OK);
	assert_int_equal(tac_create(&list), TAC_OK);

	/* TAC reference:
	   t12 = MOVE 4.2
	   t13 = MOVE t12
	*/
	assert_int_equal(tac_append(list, &instr), TAC_OK);
	instr.dst.value.id = 13;
	instr.src1.kind = TAC_OPERAND_TEMP;
	instr.src1.value.id = 12;
	assert_int_equal(tac_append(list, &instr), TAC_OK);

	assert_int_equal(optimizer_first_pass(opt, list), OPT_OK);
	opt_temp_data_t out_data = {.propag_type = OPT_PROP_DYNAMIC};
	assert_true(HTBL_RETRIEVE(opt->tmp_op_lut, "13", out_data)); /* t13 check */
	assert_int_equal(out_data.propag_type, OPT_PROP_LITERAL);

	assert_true(HTBL_RETRIEVE(opt->tmp_op_lut, "12", out_data)); /* t12 check */
	assert_int_equal(out_data.propag_type, OPT_PROP_LITERAL);

	assert_int_equal(optimizer_free(&opt), OPT_OK);
	tac_destroy(&list);
}

static tac_t *global_list_1; /* contains 2 temp operand assignments and 3 non-temp operand operations */
static tac_t *global_list_2; /* contains 3 MOVE instructions */

static void optimizer_first_pass_skips_non_assignment(void **state) {
	(void)state;
	optimizer_t *opt;
	assert_int_equal(optimizer_init(&opt), OPT_OK);
	assert_int_equal(optimizer_first_pass(opt, global_list_1), OPT_OK);

	size_t total_count = 0;
	for (size_t i = 0; i < opt->tmp_op_lut.table->size; i++) {
		if (opt->tmp_op_lut.table->array[i] != NULL)
		    total_count++;
	}
	assert_uint_equal(total_count, 2);
	assert_int_equal(optimizer_free(&opt), OPT_OK);
}

static void optimizer_first_pass_propagates_symbols(void **state) {
	(void)state;
	optimizer_t *opt;
	assert_int_equal(optimizer_init(&opt), OPT_OK);
	assert_int_equal(optimizer_first_pass(opt, global_list_2), OPT_OK);

	opt_temp_data_t out_data;
	assert_true(HTBL_RETRIEVE(opt->tmp_op_lut, "2", out_data));
	assert_int_equal(out_data.propag_type, OPT_PROP_SYMBOL);
	assert_int_equal(optimizer_free(&opt), OPT_OK);
}

/*================================
  Optimizer code-editing functions
  ================================*/
static void opt_delete_redundant_move_ignores_non_move(void **state) {
	(void)state;

	
}

/*
  The device
*/
int main(void) {
	int test_results = 0;

	/* Global list 1 init. */
	/* TAC reference:
	   0  FUNCTION_BEGIN iterate
	   1  s17 = MOVE 0
	   2  t1 = MOVE 3
	   3  t2 = MOVE 1
	   4  L1:
	*/
	tac_create(&global_list_1);
	tac_append(global_list_1, &(tac_instruction_t){.opcode = TAC_FUNCTION_BEGIN, .name = "iterate"});
	tac_append(global_list_1,
	           &(tac_instruction_t){.opcode = TAC_MOVE,
	                                .dst = {.kind = TAC_OPERAND_SYMBOL, .value.id = 17},
	                                .src1 = {.kind = TAC_OPERAND_INTEGER, .value.integer = 0}});
	tac_append(global_list_1,
	           &(tac_instruction_t){.opcode = TAC_MOVE,
	                                .dst = {.kind = TAC_OPERAND_TEMP, .value.id = 1},
	                                .src1 = {.kind = TAC_OPERAND_INTEGER, .value.integer = 3}});
	tac_append(global_list_1,
	           &(tac_instruction_t){.opcode = TAC_MOVE,
	                                .dst = {.kind = TAC_OPERAND_TEMP, .value.id = 2},
	                                .src1 = {.kind = TAC_OPERAND_INTEGER, .value.integer = 1}});
	tac_append(global_list_1, &(tac_instruction_t){.opcode = TAC_LABEL, .label = 1});

	/* Global list 2 init. */
	/* TAC reference:
	   0  s1 = MOVE "hello"
	   1  t1 = MOVE 10
	   2  t2 = MOVE s1
	*/
	tac_create(&global_list_2);
	tac_append(global_list_2,
	           &(tac_instruction_t){.opcode = TAC_MOVE,
	                                .dst = {.kind = TAC_OPERAND_SYMBOL, .value.id = 1},
	                                .src1 = {.kind = TAC_OPERAND_STRING, .value.string = "hello"}});
	tac_append(global_list_2,
	           &(tac_instruction_t){.opcode = TAC_MOVE,
	                                .dst = {.kind = TAC_OPERAND_TEMP, .value.id = 1},
	                                .src1 = {.kind = TAC_OPERAND_INTEGER, .value.integer = 10}});
	tac_append(global_list_2,
	           &(tac_instruction_t){.opcode = TAC_MOVE,
	                                .dst = {.kind = TAC_OPERAND_TEMP, .value.id = 2},
	                                .src1 = {.kind = TAC_OPERAND_SYMBOL, .value.id = 1}});


	struct CMUnitTest optimizer_alloc_tests[] = {
	    cmocka_unit_test(optimizer_init_returns),
	    cmocka_unit_test(optimizer_init_wipes_previous),
	    cmocka_unit_test(optimizer_free_returns),
	    cmocka_unit_test(optimizer_free_nulls_argument),
	};
	EXEC_TEST_CATEGORY("Optimizer allocation", optimizer_alloc_tests);

	struct CMUnitTest optimizer_helper_tests[] = {
	    cmocka_unit_test(make_operand_ident_returns),
	    cmocka_unit_test(make_operand_ident_max_value_terminates_string),
	    cmocka_unit_test(make_operand_ident_improper_size_no_overflow),
	    cmocka_unit_test(make_operand_ident_size_matches_str_len),
	    cmocka_unit_test(instr_shallow_copy_full_bore_test),
	    cmocka_unit_test(opt_is_assignment_opcode_valid_types),
	    cmocka_unit_test(opt_is_assignment_opcode_invalid_types),
	    cmocka_unit_test(opt_is_literal_operand_valid_types),
	    cmocka_unit_test(opt_is_literal_operand_invalid_types),
	};
	EXEC_TEST_CATEGORY("Optimizer helper functions", optimizer_helper_tests);

	struct CMUnitTest optimizer_get_operand_prop_tests[] = {
	    cmocka_unit_test(get_operand_propagation_type_returns),
	    cmocka_unit_test(get_operand_propagation_type_single_literal),
	    cmocka_unit_test(get_operand_propagation_type_all_literals),
	    cmocka_unit_test(get_operand_propagation_type_symbol),
	    cmocka_unit_test(get_operand_propagation_type_none),
	    cmocka_unit_test(get_operand_propagation_type_undefined_temp_src),
	    cmocka_unit_test(get_operand_propagation_type_defined_temp_src),
	};
	EXEC_TEST_CATEGORY("Operand propagation type derivation", optimizer_get_operand_prop_tests);

	struct CMUnitTest optimizer_scanner_tests[] = {
	    cmocka_unit_test(scan_temporary_operand_skip),
	    cmocka_unit_test(scan_temporary_operand_single_non_move),
	    cmocka_unit_test(scan_temporary_operand_single_move),
	    cmocka_unit_test(scan_temporary_operand_redefinition),
	};
	EXEC_TEST_CATEGORY("Optimizer scanner tests", optimizer_scanner_tests);

	struct CMUnitTest optimizer_first_pass_tests[] = {
	    cmocka_unit_test(optimizer_first_pass_empty_list),
	    cmocka_unit_test(optimizer_first_pass_single_instruction),
	    cmocka_unit_test(optimizer_first_pass_infers_propagation),
	    cmocka_unit_test(optimizer_first_pass_skips_non_assignment),
	    cmocka_unit_test(optimizer_first_pass_propagates_symbols),
	};
	EXEC_TEST_CATEGORY("Optimizer first pass tests", optimizer_first_pass_tests);

	tac_destroy(&global_list_1);
	tac_destroy(&global_list_2);
	return test_results;
}
