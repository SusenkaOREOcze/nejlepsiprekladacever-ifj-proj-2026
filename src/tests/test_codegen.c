/**
   @file test_codegen.c
   @author xbosakd00 (DasBean125)
   @date 2026-11-26
   @brief Tests the code generator module, codegen.c.

   Shamefully steals the static AST helper functions from @ref test_ast_irgen_tac.c and @ref test_irgen.c.
*/

#define UNIT_UNDER_TEST
#include "../include/codegen_impl.h"
#include "../include/irgen.h"
#include "cmocka/cmocka.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>

#define EXEC_TEST_CATEGORY(name, tests)                                                                      \
	do {                                                                                                     \
		fprintf(outfile, "\n[===" name "===]\n");                                                            \
		printf("\n[    " name "    ]\n");                                                                    \
		test_results |= (cmocka_run_group_tests_name((name), (tests), NULL, NULL));                          \
	} while (0)

#define OUTFILE_WRITE(text) fprintf(outfile, "[ %s ]\n", text)

#define OP_LITERAL(k, t, v)                                                                                  \
	(tac_operand_t) { .kind = (k), .type = (t), .value.v }

/**
 * @brief Creates an AST node with resolved metadata.
 * @param kind Node kind.
 * @param type Resolved type.
 * @param id Symbol ID.
 * @param text Optional text.
 * @return New unattached node.
 */
static ast_node_t *make_node(ast_node_kind_t kind, symt_data_type_t type, uint64_t id, char *text) {
	ast_node_t source = {.kind = kind, .data_type = type, .symbol_id = id, .text = text};
	ast_node_t *copy = NULL;
	assert_int_equal(ast_node_create(&source, &copy), AST_RESULT_OK);
	return copy;
}

/**
 * @brief Attaches one child.
 * @param parent Parent node.
 * @param child Unattached child.
 */
static void child(ast_node_t *parent, ast_node_t *child) {
	assert_int_equal(ast_node_add_child(parent, child), AST_RESULT_OK);
}

/**
 * @brief Creates an integer literal.
 * @param value Integer value.
 * @return New literal node.
 */
static ast_node_t *number(int64_t value) {
	ast_node_t *n = make_node(AST_NODE_INTEGER, SYMT_TYPE_INTEGER, AST_UNRESOLVED_SYMBOL, NULL);
	n->value.integer = value;
	return n;
}

/**
 * @brief Wraps a function in a complete program tree.
 * @param fn Function node.
 * @return Owning AST.
 */
static ast_t *tree_with(ast_node_t *fn) {
	ast_t *tree = NULL;
	ast_node_t *root = make_node(AST_NODE_PROGRAM, SYMT_TYPE_UNIT, AST_UNRESOLVED_SYMBOL, NULL);
	child(root, fn);
	assert_int_equal(ast_create(&tree), AST_RESULT_OK);
	assert_int_equal(ast_set_root(tree, root), AST_RESULT_OK);
	return tree;
}

/*======================
  Public interface tests
  ======================*/
static void test_codegen_alloc_interface(void **state) {
	(void)state;
	codegen_t *codegen = (void *)0xDEADFEED;
	funct_t *functable = NULL;
	assert_int_equal(funct_init(&functable), SYMTSTAT_OK);

	assert_int_equal(codegen_init(&codegen, NULL), CDG_BAD_ARG);
	assert_int_equal(codegen_init(NULL, functable), CDG_BAD_ARG);
	assert_int_equal(codegen_init(&codegen, functable), CDG_OK);
	assert_ptr_not_equal(codegen, (void *)0xDEADFEED);

	assert_ptr_equal(codegen->func_table, functable);
	assert_false(codegen->in_local_scope);
	assert_ptr_equal(codegen->stream, stdout);
	assert_uint_equal(codegen->temp_acc, 0);
	assert_uint_equal(codegen->sym_acc, 0);
	assert_uint_equal(codegen->loc_temp_acc, 0);
	assert_uint_equal(codegen->loc_sym_acc, 0);
	assert_uint_equal(codegen->int_temp_acc, 0);
	assert_uint_equal(codegen->int_label_acc, 0);

	assert_int_equal(funct_free(functable), SYMTSTAT_OK);
	HTBL_FREE(codegen->addr_table);
	free(codegen);
}

static void test_codegen_free_interface(void **state) {
	(void)state;
	codegen_t *codegen = (void *)0xDEADFEED;
	funct_t *functable = (void *)0xDEADBEEF;

	assert_int_equal(codegen_init(&codegen, functable), CDG_OK);

	assert_int_equal(codegen_free(NULL), CDG_BAD_ARG);
	assert_int_equal(codegen_free(&codegen), CDG_OK);
	assert_null(codegen);
}

static void test_codegen_run_interface(void **state) {
	(void)state;
	codegen_t *codegen = (void *)0xDEADBEEF;
	tac_t *code = (void *)0xDEADFEED;

	assert_int_equal(codegen_run(NULL, NULL, code), CDG_BAD_ARG); /* bad arg because of 2nd arg */
	assert_int_equal(codegen_run(stdout, codegen, NULL), CDG_BAD_ARG);
}

/*=======================
  Internal function tests
  =======================*/
FILE *outfile;
static void *const NONNULL_INVALID_PTR =
    (void *)0xDEADBEEF; /* Hey OS? Wouldn't it be funny if I just... did this? */

static void test_cdg_param_arr_allocation_basic(void **state) {
	(void)state;
	codegen_t *gen;
	assert_int_equal(codegen_init(&gen, NONNULL_INVALID_PTR), CDG_OK);
	assert_null(gen->param_stack);
	assert_uint_equal(gen->pstack_size, 0);
	assert_uint_equal(gen->pstack_count, 0);

	/* Invalid args */
	assert_int_equal(cdg_alloc_param_stack(NULL, 24), CDG_BAD_ARG);
	assert_int_equal(cdg_alloc_param_stack(gen, 0), CDG_BAD_ARG);

	/* New stack allocation */
	assert_int_equal(cdg_alloc_param_stack(gen, 4), CDG_OK);
	assert_non_null(gen->param_stack);
	assert_uint_equal(gen->pstack_size, 4);
	assert_uint_equal(gen->pstack_count, 0);

	/* This simultaneously tests stack freeing */
	codegen_free(&gen);
}

static void test_cdg_param_arr_realloc_increases_size(void **state) {
	(void)state;
	codegen_t *gen;
	assert_int_equal(codegen_init(&gen, NONNULL_INVALID_PTR), CDG_OK);
	assert_int_equal(cdg_alloc_param_stack(gen, 4), CDG_OK);
	const size_t old_size = gen->pstack_size;

	/* identical size -> do nothing */
	assert_int_equal(cdg_alloc_param_stack(gen, 4), CDG_OK);
	assert_uint_equal(old_size, gen->pstack_size);

	/* Modify sizes */
	assert_int_equal(cdg_alloc_param_stack(gen, 8), CDG_OK);
	assert_uint_equal(gen->pstack_size, 8);
	assert_int_equal(cdg_alloc_param_stack(gen, 2), CDG_OK);
	assert_uint_equal(gen->pstack_size, 2);

	codegen_free(&gen);
}

static const tac_operand_t EMPTY_OPERAND = {
    .kind = TAC_OPERAND_NONE, .type = SYMT_TYPE_UNDEFINED, .value.integer = 0};

static void test_cdg_param_arr_append_basic(void **state) {
	(void)state;
	codegen_t *gen;
	assert_int_equal(codegen_init(&gen, NONNULL_INVALID_PTR), CDG_OK);
	assert_int_equal(cdg_alloc_param_stack(gen, 4), CDG_OK);

	assert_int_equal(cdg_append_param(NULL, NONNULL_INVALID_PTR), CDG_BAD_ARG);
	assert_int_equal(cdg_append_param(gen, NULL), CDG_BAD_ARG);
	assert_int_equal(cdg_append_param(gen, &OP_LITERAL(TAC_OPERAND_TEMP, SYMT_TYPE_INTEGER, id = 2)), CDG_OK);

	assert_int_equal(gen->param_stack[0].kind, TAC_OPERAND_TEMP);
	assert_int_equal(gen->param_stack[0].type, SYMT_TYPE_INTEGER);
	assert_uint_equal(gen->param_stack[0].value.id, 2);

	codegen_free(&gen);
}

/**
   @brief Test that appending an operand to a parameter stack allocates it if it doesn't exist.
*/
static void test_cdg_param_arr_append_to_null(void **state) {
	(void)state;
	codegen_t *gen;
	assert_int_equal(codegen_init(&gen, NONNULL_INVALID_PTR), CDG_OK);
	assert_null(gen->param_stack);

	assert_int_equal(cdg_append_param(gen, &OP_LITERAL(TAC_OPERAND_TEMP, SYMT_TYPE_INTEGER, id = 4)), CDG_OK);
	assert_uint_equal(gen->pstack_size, 4); /* initial size is 4, see codegen.c */
	assert_non_null(gen->param_stack);

	codegen_free(&gen);
}

/**
   @brief Tests that resizing caused by appending params grows in multiples of 2.
*/
static void test_cdg_param_arr_append_resize_is_geometric_series(void **state) {
	(void)state;
	codegen_t *gen;
	assert_int_equal(codegen_init(&gen, NONNULL_INVALID_PTR), CDG_OK);
	const size_t base_size = 4;
	assert_int_equal(cdg_alloc_param_stack(gen, base_size), CDG_OK);

	/* Add 5 operands, forcing resize on the fifth insertion */
	for (int i = 0; i < 5; i++)
		assert_int_equal(cdg_append_param(gen, &OP_LITERAL(TAC_OPERAND_SYMBOL, SYMT_TYPE_DOUBLE, id = i)),
		                 CDG_OK);
	assert_uint_equal(gen->pstack_size, base_size * 2);

	/* Add 4 more operands, forcing a resize on the fourth */
	for (int i = 5; i < 9; i++)
		assert_int_equal(cdg_append_param(gen, &OP_LITERAL(TAC_OPERAND_SYMBOL, SYMT_TYPE_DOUBLE, id = i)),
		                 CDG_OK);
	assert_uint_equal(gen->pstack_size, base_size * 2 * 2);

	codegen_free(&gen);
}

static void test_cdg_param_arr_clear(void **state) {
	(void)state;
	codegen_t *gen;
	assert_int_equal(codegen_init(&gen, NONNULL_INVALID_PTR), CDG_OK);
	assert_int_equal(cdg_alloc_param_stack(gen, 4), CDG_OK);

	for (int i = 0; i < 3; i++)
		assert_int_equal(cdg_append_param(gen, &OP_LITERAL(TAC_OPERAND_SYMBOL, SYMT_TYPE_INTEGER, id = i)),
		                 CDG_OK);
	assert_int_equal(cdg_clear_param_stack(gen), CDG_OK);
	for (int i = 0; i < (int)gen->pstack_size; i++)
		assert_memory_equal(&gen->param_stack[i], &EMPTY_OPERAND, sizeof(EMPTY_OPERAND));

	codegen_free(&gen);
}

/*===================
  Codegen print tests
  ===================*/
#define CODEGEN_INIT_NO_FUNCT(gen)                                                                           \
	assert_int_equal(codegen_init(&gen, NONNULL_INVALID_PTR), CDG_OK);                                       \
	gen->stream = outfile

#define CDG_NEWLINE(gen) fputc('\n', gen->stream)

/**
   @brief Prints all five types of literals to outfile.
*/
static void test_cdg_print_literal(void **state) {
	OUTFILE_WRITE(__PRETTY_FUNCTION__);
	(void)state;
	codegen_t *gen;
	CODEGEN_INIT_NO_FUNCT(gen);

	assert_int_equal(cdg_print_literal(gen, &OP_LITERAL(TAC_OPERAND_NONE, SYMT_TYPE_UNIT, integer = 0)),
	                 CDG_INTERNAL_FAILURE);

	/* expect: nil@nil */
	assert_int_equal(cdg_print_literal(gen, &OP_LITERAL(TAC_OPERAND_UNIT, SYMT_TYPE_UNIT, integer = 0)),
	                 CDG_OK);
	CDG_NEWLINE(gen);

	/* expect: int@-42 */
	assert_int_equal(
	    cdg_print_literal(gen, &OP_LITERAL(TAC_OPERAND_INTEGER, SYMT_TYPE_INTEGER, integer = -42)), CDG_OK);
	CDG_NEWLINE(gen);

	/* expect: float@0x1.b99999999999ap+2 (floating point value of 6.9) */
	assert_int_equal(cdg_print_literal(gen, &OP_LITERAL(TAC_OPERAND_DOUBLE, SYMT_TYPE_DOUBLE, decimal = 6.9)),
	                 CDG_OK);
	CDG_NEWLINE(gen);

	/* expect: string@hello\032world\035 */
	assert_int_equal(
	    cdg_print_literal(gen, &OP_LITERAL(TAC_OPERAND_STRING, SYMT_TYPE_STRING, string = "hello world#")),
	    CDG_OK);
	CDG_NEWLINE(gen);

	/* expect: bool@true */
	assert_int_equal(
	    cdg_print_literal(gen, &OP_LITERAL(TAC_OPERAND_BOOLEAN, SYMT_TYPE_BOOLEAN, boolean = true)), CDG_OK);
	CDG_NEWLINE(gen);

	assert_int_equal(codegen_free(&gen), CDG_OK);
}

#define ASS_PRINT_OPERAND(operand, flg) assert_int_equal(cdg_print_operand(gen, operand, flg), CDG_OK)
#define ASS_PRINT_DEFVAR(operand, inter) assert_int_equal(cdg_print_defvar(gen, operand, inter), CDG_OK)

/**
   @brief Test that printing a temporary operand stores its address in the address table.
*/
static void test_cdg_print_temp_operand_writes_address(void **state) {
	OUTFILE_WRITE(__PRETTY_FUNCTION__);
	(void)state;
	codegen_t *gen;
	CODEGEN_INIT_NO_FUNCT(gen);

	/* No output */
	assert_int_equal(cdg_print_operand(NULL, NONNULL_INVALID_PTR, CDG_PFLAG_NONE), CDG_BAD_ARG);
	assert_int_equal(cdg_print_operand(gen, NULL, CDG_PFLAG_NONE), CDG_BAD_ARG);
	assert_int_equal(
	    cdg_print_operand(gen, &OP_LITERAL(TAC_OPERAND_NONE, SYMT_TYPE_BOOLEAN, id = 0), CDG_PFLAG_NONE),
	    CDG_INVALID_OPERAND);
	assert_int_equal(cdg_print_operand(gen, &OP_LITERAL(TAC_OPERAND_SYMBOL, SYMT_TYPE_DOUBLE, id = 2),
	                                   CDG_PFLAG_INTERNAL_NAMESPACE),
	                 CDG_INVALID_OPERAND); /* symbol in internal namespace is banned */

	/* Expect: GF@t0 */
	ASS_PRINT_OPERAND(&OP_LITERAL(TAC_OPERAND_TEMP, SYMT_TYPE_INTEGER, id = 0), CDG_PFLAG_APPEND_LINE_FEED);
	cdg_address_t addr;
	assert_true(HTBL_RETRIEVE(gen->addr_table, "t0", addr));
	assert_int_equal(addr.type, TAC_OPERAND_TEMP);
	assert_false(addr.is_defined);
	assert_false(addr.is_local);
	assert_uint_equal(addr.code_id, 0);

	codegen_free(&gen);
}

/**
   @brief Tests that printing a local-scope temporary operand followed by an internal temporary operand uses
   the correct ID.
*/
static void test_cdg_print_temp_operands_different_namespace(void **state) {
	OUTFILE_WRITE(__PRETTY_FUNCTION__);
	(void)state;
	codegen_t *gen;
	CODEGEN_INIT_NO_FUNCT(gen);
	gen->in_local_scope = true; /* move to LF */
	gen->loc_temp_acc = 42;     /* local temp will have id 42 */
	gen->int_temp_acc = 69;     /* internal temp will have id 69 */

	/* Expect: LF@t42, LF@ti69 */
	ASS_PRINT_OPERAND(&OP_LITERAL(TAC_OPERAND_TEMP, SYMT_TYPE_INTEGER, id = 0), CDG_PFLAG_APPEND_LINE_FEED);
	ASS_PRINT_OPERAND(&OP_LITERAL(TAC_OPERAND_TEMP, SYMT_TYPE_DOUBLE, id = 0),
	                  CDG_PFLAG_INTERNAL_NAMESPACE | CDG_PFLAG_APPEND_LINE_FEED);
	assert_true(HTBL_EXISTS(gen->addr_table, "t0")); /* note that addresses are saved with TAC ID keys */
	assert_true(HTBL_EXISTS(gen->addr_table, "ti0"));

	codegen_free(&gen);
}

static void test_cdg_print_symbol_in_temp_frame(void **state) {
	OUTFILE_WRITE(__PRETTY_FUNCTION__);
	(void)state;
	codegen_t *gen;
	CODEGEN_INIT_NO_FUNCT(gen);

	/* Expect: TF@s0 */
	ASS_PRINT_OPERAND(&OP_LITERAL(TAC_OPERAND_SYMBOL, SYMT_TYPE_STRING, id = 12),
	                  CDG_PFLAG_APPEND_LINE_FEED | CDG_PFLAG_DECL_TEMP);
	cdg_address_t addr;
	assert_true(HTBL_RETRIEVE(gen->addr_table, "s12", addr)); /* written TAC id was 12! */
	assert_int_equal(addr.type, TAC_OPERAND_SYMBOL);
	assert_false(addr.is_defined);
	assert_false(addr.is_local);
	assert_uint_equal(addr.code_id, 0);

	codegen_free(&gen);
}

/**
   @brief Test that printing two operands with the same identifier reuses the code ID defined by the first
   print.

   @par Expected output:
   @code
   GF\\@s0
   GF\\@s0 	# reused ID here
   @endcode
*/
static void test_cdg_print_reuses_existing_id(void **state) {
	OUTFILE_WRITE(__PRETTY_FUNCTION__);
	(void)state;
	codegen_t *gen;
	CODEGEN_INIT_NO_FUNCT(gen);

	/* Expect: GF@s0 */
	/* NOTE: the TAC code's operand ID is 1! */
	ASS_PRINT_OPERAND(&OP_LITERAL(TAC_OPERAND_SYMBOL, SYMT_TYPE_INTEGER, id = 1), CDG_PFLAG_APPEND_LINE_FEED);
	const unsigned long old_sym_accum = gen->sym_acc; /* will be 1 */
	ASS_PRINT_OPERAND(&OP_LITERAL(TAC_OPERAND_SYMBOL, SYMT_TYPE_INTEGER, id = 1), CDG_PFLAG_APPEND_LINE_FEED);
	assert_uint_equal(old_sym_accum,
	                  1); /* must remain 1, since the other print should reuse the existing ID */

	cdg_address_t addr;
	assert_true(HTBL_RETRIEVE(gen->addr_table, "s1", addr));
	assert_int_equal(addr.type, TAC_OPERAND_SYMBOL);
	assert_false(addr.is_defined);
	assert_false(addr.is_local);
	assert_uint_equal(addr.code_id, 0); /* should remain zero */

	codegen_free(&gen);
}

/**
   @brief Test that writing several different operands uses correct identifiers (based on scope and type).

   @par Expected output:
   @code
   LF\\@t0 GF\\@ti2
   TF\\@s4
   @endcode
*/
static void test_cdg_print_combined_types_unique_id(void **state) {
	OUTFILE_WRITE(__PRETTY_FUNCTION__);
	(void)state;
	codegen_t *gen;
	CODEGEN_INIT_NO_FUNCT(gen);
	gen->loc_temp_acc = 0;
	gen->int_temp_acc = 2;
	gen->sym_acc = 4;

	gen->in_local_scope = true;
	ASS_PRINT_OPERAND(&OP_LITERAL(TAC_OPERAND_TEMP, SYMT_TYPE_BOOLEAN, id = 0),
	                  CDG_PFLAG_APPEND_SPACE | CDG_PFLAG_INTERNAL_NAMESPACE);
	gen->in_local_scope = false;
	ASS_PRINT_OPERAND(&OP_LITERAL(TAC_OPERAND_TEMP, SYMT_TYPE_BOOLEAN, id = 0), CDG_PFLAG_APPEND_LINE_FEED);
	ASS_PRINT_OPERAND(&OP_LITERAL(TAC_OPERAND_SYMBOL, SYMT_TYPE_INTEGER, id = 0),
	                  CDG_PFLAG_APPEND_LINE_FEED | CDG_PFLAG_DECL_TEMP);

	codegen_free(&gen);
}

/**
   @brief Test if defining a new symbol properly writes its address.

   This also tests using invalid arguments.
*/
static void test_cdg_print_defvar_writes_address_if_undef(void **state) {
	OUTFILE_WRITE(__PRETTY_FUNCTION__);
	(void)state;
	codegen_t *gen;
	tac_operand_t op = {0};
	CODEGEN_INIT_NO_FUNCT(gen);

	/* Invalid operands test cuz why not */
	assert_int_equal(cdg_print_defvar(NULL, &op, false), CDG_BAD_ARG);
	assert_int_equal(cdg_print_defvar(gen, NULL, true), CDG_BAD_ARG);
	assert_int_equal(
	    cdg_print_defvar(gen, &OP_LITERAL(TAC_OPERAND_DOUBLE, SYMT_TYPE_DOUBLE, decimal = 0), false),
	    CDG_INVALID_OPERAND);

	/* Expect: DEFVAR GF@t0 (from TAC: t4) */
	ASS_PRINT_DEFVAR(&OP_LITERAL(TAC_OPERAND_TEMP, SYMT_TYPE_INTEGER, id = 4), false);
	cdg_address_t addr;
	assert_true(HTBL_RETRIEVE(gen->addr_table, "t4", addr));
	assert_int_equal(addr.type, TAC_OPERAND_TEMP);
	assert_true(addr.is_defined); /* this is the important bit */
	assert_false(addr.is_local);
	assert_uint_equal(addr.code_id, 0);

	codegen_free(&gen);
}

/**
   @brief Test that calling cdg_print_defvar consecutively on the same symbol prints it only the first time.

   @par Expected output:
   @code
   DEFVAR GF\\@t0
   DEFVAR GF\\@t1
   @endcode
*/
static void test_cdg_print_defvar_skips_already_defined(void **state) {
	OUTFILE_WRITE(__PRETTY_FUNCTION__);
	(void)state;
	codegen_t *gen;
	CODEGEN_INIT_NO_FUNCT(gen);

	ASS_PRINT_DEFVAR(&OP_LITERAL(TAC_OPERAND_TEMP, SYMT_TYPE_INTEGER, id = 0), false);
	ASS_PRINT_DEFVAR(&OP_LITERAL(TAC_OPERAND_TEMP, SYMT_TYPE_INTEGER, id = 2), false);
	ASS_PRINT_DEFVAR(&OP_LITERAL(TAC_OPERAND_TEMP, SYMT_TYPE_INTEGER, id = 0), false);

	codegen_free(&gen);
}

/**
   @brief Test defining an internal operand followed by a normal operand.

   @par Expected output:
   @code
   DEFVAR GF\\@ti0
   DEFVAR GF\\@t0
   @endcode
*/
static void test_cdg_print_defvar_writes_internal_operand(void **state) {
	OUTFILE_WRITE(__PRETTY_FUNCTION__);
	(void)state;
	codegen_t *gen;
	CODEGEN_INIT_NO_FUNCT(gen);

	ASS_PRINT_DEFVAR(&OP_LITERAL(TAC_OPERAND_TEMP, SYMT_TYPE_BOOLEAN, id = 0), true);  /* internal */
	ASS_PRINT_DEFVAR(&OP_LITERAL(TAC_OPERAND_TEMP, SYMT_TYPE_BOOLEAN, id = 0), false); /* normal */

	codegen_free(&gen);
}

/*========================
  Codegen generation tests
  ========================*/
#define INTEGER(val) {.kind = TAC_OPERAND_INTEGER, .type = SYMT_TYPE_INTEGER, .value.integer = (val)}
#define DECIMAL(val) {.kind = TAC_OPERAND_DOUBLE, .type = SYMT_TYPE_DOUBLE, .value.decimal = (val)}
#define BOOL(val) {.kind = TAC_OPERAND_BOOLEAN, .type = SYMT_TYPE_BOOLEAN, .value.boolean = (val)}
#define UNIT(val) {.kind = TAC_OPERAND_UNIT, .type = SYMT_TYPE_UNIT, .value.integer = 0}
#define STRING(val) {.kind = TAC_OPERAND_STRING, .type = SYMT_TYPE_STRING, .value.string = (val)}
#define OPR_TEMP(ident, t) {.kind = TAC_OPERAND_TEMP, .type = SYMT_TYPE_##t, .value.id = (ident)}
#define OPR_SYMB(ident, t) {.kind = TAC_OPERAND_SYMBOL, .type = SYMT_TYPE_##t, .value.id = (ident)}

#define UNARY_INSTR(opc, dest, source)                                                                       \
	(tac_instruction_t) { .opcode = (opc), .dst = dest, .src1 = source }
#define BINARY_INSTR(opc, dest, source1, source2)                                                            \
	(tac_instruction_t) { .opcode = (opc), .dst = dest, .src1 = source1, .src2 = source2 }
#define TAC_ASS_APPEND(instr) assert_int_equal(tac_append(code, instr), TAC_OK)

/**
   @brief Generate several MOVE instructions, assigning literals to temporaries. Includes a temporary
   redefinition test.

   @par Expected output:
   @code
   DEFVAR GF\\@t0
   MOVE GF\\@t0 int@43
   DEFVAR GF\\@t1
   MOVE GF\\@t1 float\\@0x1.14p+6
   DEFVAR GF\\@t2
   MOVE GF\\@t2 string\\@Nazdar\032svete\010\032novej radek vole!
   MOVE GF\\@t0 int@42
   @endcode
*/
static void test_cdg_run_move_literal_to_temp_several(void **state) {
	OUTFILE_WRITE(__PRETTY_FUNCTION__);
	(void)state;
	codegen_t *codegen;
	funct_t *funct = NONNULL_INVALID_PTR; /* won't be using funct here */
	tac_t *code = NULL;
	assert_int_equal(tac_create(&code), TAC_OK);

	/* TAC: t0 = MOVE 43 */
	TAC_ASS_APPEND(&UNARY_INSTR(TAC_MOVE, OPR_TEMP(0, INTEGER), INTEGER(43)));

	/* TAC: t1 = MOVE 69.0 */
	TAC_ASS_APPEND(&UNARY_INSTR(TAC_MOVE, OPR_TEMP(1, DOUBLE), DECIMAL(69)));

	/* TAC: t2 = MOVE "Nazdar sevete\n novej radek vole!" */
	TAC_ASS_APPEND(&UNARY_INSTR(TAC_MOVE, OPR_TEMP(2, STRING), STRING("Nazdar sevete\n novej radek vole!")));

	/* TAC: t0 = MOVE 42, should not cause a DEFVAR, because t0 already exists */
	TAC_ASS_APPEND(&UNARY_INSTR(TAC_MOVE, OPR_TEMP(0, INTEGER), INTEGER(42)));

	assert_int_equal(codegen_init(&codegen, funct), CDG_OK);
	assert_int_equal(codegen_run(outfile, codegen, code), CDG_OK);

	tac_destroy(&code);
	codegen_free(&codegen);
}

/**
   @brief Generate move instructions in local frame.

   @par Expected output:
   @code
   DEFVAR LF\\@$0
   MOVE LF\\@$0 int\\@64
   DEFVAR LF\\@t0
   MOVE LF\\@t0 LF\\@s0
   DEFVAR LF\\@t1
   MOVE LF\\@t1 LF\\@t0
   @endcode
*/
static void test_cdg_run_move_var_to_var(void **state) {
	OUTFILE_WRITE(__PRETTY_FUNCTION__);
	(void)state;
	codegen_t *gen;
	funct_t *funct = NONNULL_INVALID_PTR; /* won't be using funct here */
	tac_t *code = NULL;
	assert_int_equal(tac_create(&code), TAC_OK);
	assert_int_equal(codegen_init(&gen, funct), CDG_OK);

	TAC_ASS_APPEND(&UNARY_INSTR(TAC_MOVE, OPR_SYMB(0, INTEGER), INTEGER(64)));
	TAC_ASS_APPEND(&UNARY_INSTR(TAC_MOVE, OPR_TEMP(0, INTEGER), OPR_SYMB(0, INTEGER)));
	TAC_ASS_APPEND(&UNARY_INSTR(TAC_MOVE, OPR_TEMP(1, INTEGER), OPR_TEMP(0, INTEGER)));

	gen->in_local_scope = true;
	assert_int_equal(codegen_run(outfile, gen, code), CDG_OK);
	cdg_address_t addr;
	HTBL_RETRIEVE(gen->addr_table, "t0", addr);
	assert_true(addr.is_local);

	tac_destroy(&code);
	codegen_free(&gen);
}

/**
   @brief Generate different kinds of simple binary instructions (ADD, MUL, SUB, etc).

   The printed code is just a set of all of the possible results.
*/
static void test_cdg_run_simple_binary_instructions(void **state) {
	OUTFILE_WRITE(__PRETTY_FUNCTION__);
	(void)state;
	codegen_t *gen;
	funct_t *funct = NONNULL_INVALID_PTR; /* won't be using funct here */
	tac_t *code = NULL;
	assert_int_equal(tac_create(&code), TAC_OK);
	assert_int_equal(codegen_init(&gen, funct), CDG_OK);

	static const tac_opcode_t OPCODES[] = {TAC_ADD,     TAC_SUBTRACT, TAC_MULTIPLY, TAC_CONCATENATE, TAC_LESS,
	                                       TAC_GREATER, TAC_EQUAL,    TAC_OR,       TAC_AND};
	const int op_count = sizeof(OPCODES) / sizeof(tac_opcode_t);
	for (int i = 0; i < op_count; i++) {
		tac_instruction_t instr = {
		    .opcode = (OPCODES[i]),
		    .dst = {.kind = TAC_OPERAND_TEMP, .type = SYMT_TYPE_INTEGER, .value.id = (i)},
		    .src1 = {.kind = TAC_OPERAND_SYMBOL, .type = SYMT_TYPE_INTEGER, .value.id = (i)},
		    .src2 = {.kind = TAC_OPERAND_INTEGER, .type = SYMT_TYPE_INTEGER, .value.integer = (i)}};

		assert_int_equal(tac_append(code, &instr), TAC_OK);
	}

	assert_int_equal(codegen_run(outfile, gen, code), CDG_OK);
	tac_destroy(&code);
	codegen_free(&gen);
}

/**
   @brief Generate non-strict inequality instructions. These are tested because of their multi-instruction
   nature.

   @par Expected output:
   @code
   DEFVAR GF\\@ti0
   GT GF\\@ti0 GF\\@t0 int@-6
   DEFVAR GF\\@$0
   NOT GF\\@$0 GF\\@ti0
   DEFVAR GF\\@ti1
   LT GF\\@ti1 GF\\@t1 int@6
   DEFVAR GF\\@$1
   NOT GF\\@$1 GF\\@ti1
   @endcode
*/
static void test_cdg_run_nonstrict_inequalities(void **state) {
	OUTFILE_WRITE(__PRETTY_FUNCTION__);
	(void)state;
	codegen_t *gen;
	funct_t *funct = NONNULL_INVALID_PTR; /* won't be using funct here */
	tac_t *code = NULL;
	assert_int_equal(tac_create(&code), TAC_OK);
	assert_int_equal(codegen_init(&gen, funct), CDG_OK);

	TAC_ASS_APPEND(&BINARY_INSTR(TAC_LESS_EQUAL, OPR_SYMB(0, BOOLEAN), OPR_TEMP(0, INTEGER), INTEGER(-6)));
	TAC_ASS_APPEND(&BINARY_INSTR(TAC_GREATER_EQUAL, OPR_SYMB(1, BOOLEAN), OPR_TEMP(1, INTEGER), INTEGER(6)));

	assert_int_equal(codegen_run(outfile, gen, code), CDG_OK);
	tac_destroy(&code);
	codegen_free(&gen);
}

/**
   @brief Tests that TAC_NEGATIVE instructions with a literal source operand are replaced with a MOVE with the
   negative of the literal.

   @par Expected output:
   @code
   DEFVAR GF\\@$0
   MOVE GF\\@$0 float\\@-0x1.199999999999ap+2

   DEFVAR LF\\@t0
   MOVE LF\\@t0 int@6
   @endcode
*/
static void test_cdg_run_negative_precomputes_literal(void **state) {
	OUTFILE_WRITE(__PRETTY_FUNCTION__);
	(void)state;
	codegen_t *gen;
	funct_t *funct = NONNULL_INVALID_PTR; /* won't be using funct here */
	tac_t *code = NULL;
	assert_int_equal(tac_create(&code), TAC_OK);
	assert_int_equal(codegen_init(&gen, funct), CDG_OK);

	tac_instruction_t instr = UNARY_INSTR(TAC_NEGATIVE, OPR_SYMB(0, DOUBLE), DECIMAL(4.4));
	TAC_ASS_APPEND(&instr);
	assert_int_equal(codegen_run(outfile, gen, code), CDG_OK);

	gen->in_local_scope = true;
	assert_int_equal(tac_remove(code, tac_first(code)), TAC_OK);
	instr.dst = (tac_operand_t)OPR_TEMP(0, INTEGER);
	instr.src1 = (tac_operand_t)INTEGER(-6);
	TAC_ASS_APPEND(&instr);
	assert_int_equal(codegen_run(outfile, gen, code), CDG_OK);

	tac_destroy(&code);
	codegen_free(&gen);
}

/**
   @brief Test that TAC_NEGATIVE with a symbol source operand is converted to a MUL by -1 instruction.

   @par Expected output:
   @code
   DEFVAR LF\\@t0
   MUL LF\\@t0 LF\\@$0 int\\@-1
   DEFVAR LF\\@t1
   MUL LF\\@t1 LF\\@$1 float\\@-0x1p+0
   @endcode
*/
static void test_cdg_run_negative_on_symbol(void **state) {
	OUTFILE_WRITE(__PRETTY_FUNCTION__);
	(void)state;
	codegen_t *gen;
	funct_t *funct = NONNULL_INVALID_PTR; /* won't be using funct here */
	tac_t *code = NULL;
	assert_int_equal(tac_create(&code), TAC_OK);
	assert_int_equal(codegen_init(&gen, funct), CDG_OK);

	gen->in_local_scope = true;
	TAC_ASS_APPEND(&UNARY_INSTR(TAC_NEGATIVE, OPR_TEMP(0, INTEGER), OPR_SYMB(0, INTEGER)));
	TAC_ASS_APPEND(&UNARY_INSTR(TAC_NEGATIVE, OPR_TEMP(1, DOUBLE), OPR_SYMB(1, DOUBLE)));

	assert_int_equal(codegen_run(outfile, gen, code), CDG_OK);
	tac_destroy(&code);
	codegen_free(&gen);
}

/**
   @brief Test generting a floating-point operand division.

   @par Expected output:
   @code
   DEFVAR GF\\@t0
   DIV GF\\@t0 GF\\@$0 float\\@0x1.0cccccccccccdp+2
   @endcode
*/
static void test_cdg_run_float_division(void **state) {
	OUTFILE_WRITE(__PRETTY_FUNCTION__);
	(void)state;
	codegen_t *gen;
	funct_t *funct = NONNULL_INVALID_PTR; /* won't be using funct here */
	tac_t *code = NULL;
	assert_int_equal(tac_create(&code), TAC_OK);
	assert_int_equal(codegen_init(&gen, funct), CDG_OK);

	TAC_ASS_APPEND(
	    &BINARY_INSTR(TAC_DIVIDE, OPR_TEMP(29084300, DOUBLE), OPR_SYMB(69420, DOUBLE), DECIMAL(4.2)));

	assert_int_equal(codegen_run(outfile, gen, code), CDG_OK);
	tac_destroy(&code);
	codegen_free(&gen);
}

/**
   @brief Same as @ref test_cdg_run_float_division, but with an integer.

   @par Expected output:
   @code
   DEFVAR GF\\@$0
   IDIV GF\\@$0 GF\\@t0 int\\@69
   @endcode
*/
static void test_cdg_run_integer_division(void **state) {
	OUTFILE_WRITE(__PRETTY_FUNCTION__);
	(void)state;
	codegen_t *gen;
	funct_t *funct = NONNULL_INVALID_PTR; /* won't be using funct here */
	tac_t *code = NULL;
	assert_int_equal(tac_create(&code), TAC_OK);
	assert_int_equal(codegen_init(&gen, funct), CDG_OK);

	TAC_ASS_APPEND(&BINARY_INSTR(TAC_DIVIDE, OPR_SYMB(2, INTEGER), OPR_TEMP(3, INTEGER), INTEGER(69)));

	assert_int_equal(codegen_run(outfile, gen, code), CDG_OK);
	tac_destroy(&code);
	codegen_free(&gen);
}

/*=================================
  Codegen function generation tests
  =================================*/

/**
   @brief Creates a single function definition that returns Unit.

   @par Source:
   @code
   def foo(): Unit = ()
   @endcode
   @par Expected output:
   @code
   LABEL _foo
   DEFVAR LF\\@return
   POPFRAME
   MOVE TF\\@return nil\\@nil
   RETURN
   @endcode
*/
static void test_cdg_run_single_function_definition(void **state) {
	OUTFILE_WRITE(__PRETTY_FUNCTION__);
	(void)state;
	codegen_t *gen;
	funct_t *funct;
	tac_t *code = NULL;
	assert_int_equal(funct_init(&funct), SYMTSTAT_OK);
	assert_int_equal(codegen_init(&gen, funct), CDG_OK);
	char *fname = "foo";
	assert_int_equal(funct_insert_begin(funct, true), SYMTSTAT_OK);
	assert_int_equal(funct_insert_end(funct, fname, SYMT_TYPE_INTEGER), SYMTSTAT_OK);

	ast_node_t *fn = make_node(AST_NODE_FUNCTION, SYMT_TYPE_INTEGER, AST_UNRESOLVED_SYMBOL, fname);
	ast_node_t *body = make_node(AST_NODE_BLOCK, SYMT_TYPE_UNIT, AST_UNRESOLVED_SYMBOL, NULL);
	ast_node_t *ret = make_node(AST_NODE_RETURN, SYMT_TYPE_UNIT, AST_UNRESOLVED_SYMBOL, NULL);
	child(body, ret);
	child(fn, body);

	ast_t *tree = tree_with(fn);
	assert_int_equal(irgen_generate(tree, &code), IRGEN_OK);
	assert_int_equal(codegen_run(outfile, gen, code), CDG_OK);

	tac_destroy(&code);
	funct_free(funct);
	codegen_free(&gen);
	ast_destroy(&tree);
}

/**
   @brief Generate a function definition with two parameters, returning the first one.

   @par Source:
   @code
   def single_bool(cond: Boolean, a: Int): Boolean = cond
   @endcode
   @par Expected output:
   @code
   LABEL _two_param
   DEFVAR LF\\@return
   DEFVAR LF\\@$0
   POPS LF\\@$0
   DEFVAR LF\\@$1
   POPS LF\\@$1
   POPFRAME
   MOVE TF\\@return LF\\@$1
   RETURN
   @endcode
*/
static void test_cdg_run_function_definition_single_param(void **state) {
	OUTFILE_WRITE(__PRETTY_FUNCTION__);
	(void)state;
	codegen_t *gen;
	funct_t *funct;
	tac_t *code = NULL;
	assert_int_equal(funct_init(&funct), SYMTSTAT_OK);
	assert_int_equal(codegen_init(&gen, funct), CDG_OK);
	char *fname = "two_param";
	assert_int_equal(funct_insert_begin(funct, true), SYMTSTAT_OK);
	assert_int_equal(funct_insert_end(funct, fname, SYMT_TYPE_INTEGER), SYMTSTAT_OK);

	ast_node_t *fn = make_node(AST_NODE_FUNCTION, SYMT_TYPE_BOOLEAN, AST_UNRESOLVED_SYMBOL, fname);
	child(fn, make_node(AST_NODE_PARAMETER, SYMT_TYPE_BOOLEAN, 1, "cond"));
	child(fn, make_node(AST_NODE_PARAMETER, SYMT_TYPE_INTEGER, 2, "a"));
	ast_node_t *body = make_node(AST_NODE_BLOCK, SYMT_TYPE_BOOLEAN, AST_UNRESOLVED_SYMBOL, NULL);
	ast_node_t *ret = make_node(AST_NODE_RETURN, SYMT_TYPE_BOOLEAN, AST_UNRESOLVED_SYMBOL, NULL);
	child(ret, make_node(AST_NODE_IDENTIFIER, SYMT_TYPE_BOOLEAN, 1, "cond"));
	child(body, ret);
	child(fn, body);

	ast_t *tree = tree_with(fn);
	assert_int_equal(irgen_generate(tree, &code), IRGEN_OK);
	//print_tac("function two_param", code);
	assert_int_equal(codegen_run(outfile, gen, code), CDG_OK);	

	tac_destroy(&code);
	funct_free(funct);
	codegen_free(&gen);
	ast_destroy(&tree);
}


/*======================
  Full bore codegen test
  ======================*/
/**
   @brief Generates a piece of more complex code with branches.

   This is taken straight from the function test_nested_blocks_and_conditions in @ref test_ast_irgen_tac.c.
*/
static void test_cdg_generate_complex_branched(void **state) {
	OUTFILE_WRITE(__PRETTY_FUNCTION__);
	(void)state;
	codegen_t *gen;
	funct_t *funct;
	tac_t *code = NULL;
	assert_int_equal(funct_init(&funct), SYMTSTAT_OK);
	assert_int_equal(codegen_init(&gen, funct), CDG_OK);

	ast_node_t *fn = make_node(AST_NODE_MAIN_FUNCTION, SYMT_TYPE_UNIT, AST_UNRESOLVED_SYMBOL, "main");
	ast_node_t *body = make_node(AST_NODE_BLOCK, SYMT_TYPE_UNIT, AST_UNRESOLVED_SYMBOL, NULL);
	ast_node_t *decl = make_node(AST_NODE_VAR_DECLARATION, SYMT_TYPE_INTEGER, 21, "x");
	child(decl, number(0));
	child(body, decl);

	ast_node_t *outer = make_node(AST_NODE_IF, SYMT_TYPE_UNIT, AST_UNRESOLVED_SYMBOL, NULL);
	ast_node_t *outer_test = make_node(AST_NODE_LESS, SYMT_TYPE_BOOLEAN, AST_UNRESOLVED_SYMBOL, NULL);
	child(outer_test, make_node(AST_NODE_IDENTIFIER, SYMT_TYPE_INTEGER, 21, "x"));
	child(outer_test, number(3));
	child(outer, outer_test);
	ast_node_t *outer_yes = make_node(AST_NODE_BLOCK, SYMT_TYPE_UNIT, AST_UNRESOLVED_SYMBOL, NULL);
	ast_node_t *nested = make_node(AST_NODE_BLOCK, SYMT_TYPE_UNIT, AST_UNRESOLVED_SYMBOL, NULL);
	ast_node_t *inner = make_node(AST_NODE_IF, SYMT_TYPE_UNIT, AST_UNRESOLVED_SYMBOL, NULL);
	ast_node_t *inner_test = make_node(AST_NODE_EQUAL, SYMT_TYPE_BOOLEAN, AST_UNRESOLVED_SYMBOL, NULL);
	child(inner_test, make_node(AST_NODE_IDENTIFIER, SYMT_TYPE_INTEGER, 21, "x"));
	child(inner_test, number(0));
	child(inner, inner_test);

	ast_node_t *inner_yes = make_node(AST_NODE_BLOCK, SYMT_TYPE_UNIT, AST_UNRESOLVED_SYMBOL, NULL);
	ast_node_t *assign_one = make_node(AST_NODE_ASSIGNMENT, SYMT_TYPE_UNIT, AST_UNRESOLVED_SYMBOL, NULL);
	child(assign_one, make_node(AST_NODE_IDENTIFIER, SYMT_TYPE_INTEGER, 21, "x"));
	child(assign_one, number(1));
	child(inner_yes, assign_one);
	child(inner, inner_yes);

	ast_node_t *inner_no = make_node(AST_NODE_BLOCK, SYMT_TYPE_UNIT, AST_UNRESOLVED_SYMBOL, NULL);
	ast_node_t *assign_two = make_node(AST_NODE_ASSIGNMENT, SYMT_TYPE_UNIT, AST_UNRESOLVED_SYMBOL, NULL);
	child(assign_two, make_node(AST_NODE_IDENTIFIER, SYMT_TYPE_INTEGER, 21, "x"));
	child(assign_two, number(2));
	child(inner_no, assign_two);
	child(inner, inner_no);
	child(nested, inner);
	child(outer_yes, nested);
	child(outer, outer_yes);

	ast_node_t *outer_no = make_node(AST_NODE_BLOCK, SYMT_TYPE_UNIT, AST_UNRESOLVED_SYMBOL, NULL);
	ast_node_t *assign_three = make_node(AST_NODE_ASSIGNMENT, SYMT_TYPE_UNIT, AST_UNRESOLVED_SYMBOL, NULL);
	child(assign_three, make_node(AST_NODE_IDENTIFIER, SYMT_TYPE_INTEGER, 21, "x"));
	child(assign_three, number(3));
	child(outer_no, assign_three);
	child(outer, outer_no);
	child(body, outer);
	child(fn, body);
	ast_t *tree = tree_with(fn);
	assert_int_equal(irgen_generate(tree, &code), IRGEN_OK);

	assert_int_equal(codegen_run(outfile, gen, code), CDG_OK);
	
	tac_destroy(&code);
	funct_free(funct);
	codegen_free(&gen);
	ast_destroy(&tree);
}

/*
  The machine
*/
/**
 * @brief Generates NOT and inequality instructions.
 */
static void test_not_and_not_equal(void **state) {
	(void)state;
	funct_t *functions = NULL;
	codegen_t *gen = NULL;
	tac_t *code = NULL;
	FILE *output = tmpfile();
	assert_non_null(output);
	assert_int_equal(funct_init(&functions), SYMTSTAT_OK);
	assert_int_equal(codegen_init(&gen, functions), CDG_OK);
	assert_int_equal(tac_create(&code), TAC_OK);
	tac_instruction_t instruction = {
	    .opcode = TAC_NOT,
	    .dst = {.kind = TAC_OPERAND_TEMP, .type = SYMT_TYPE_BOOLEAN, .value.id = 1},
	    .src1 = {.kind = TAC_OPERAND_BOOLEAN, .type = SYMT_TYPE_BOOLEAN, .value.boolean = true}};
	assert_int_equal(tac_append(code, &instruction), TAC_OK);
	instruction.opcode = TAC_NOT_EQUAL;
	instruction.dst.value.id = 2;
	instruction.src2 = instruction.src1;
	assert_int_equal(tac_append(code, &instruction), TAC_OK);
	assert_int_equal(codegen_run(output, gen, code), CDG_OK);
	assert_int_equal(fflush(output), 0);
	assert_int_equal(fseek(output, 0, SEEK_SET), 0);
	char text[8192];
	size_t length = fread(text, 1, sizeof(text) - 1, output);
	text[length] = '\0';
	assert_non_null(strstr(text, "NOT "));
	assert_non_null(strstr(text, "EQ "));
	tac_destroy(&code);
	assert_int_equal(codegen_free(&gen), CDG_OK);
	assert_int_equal(funct_free(functions), SYMTSTAT_OK);
	assert_int_equal(fclose(output), 0);
}

/**
 * @brief Calls the custom-named entry function and emits built-ins.
 */
static void test_entrypoint_and_builtins(void **state) {
	(void)state;
	funct_t *functions = NULL;
	codegen_t *gen = NULL;
	tac_t *code = NULL;
	FILE *output = tmpfile();
	assert_non_null(output);
	assert_int_equal(funct_init(&functions), SYMTSTAT_OK);
	assert_int_equal(codegen_init(&gen, functions), CDG_OK);
	assert_int_equal(tac_create(&code), TAC_OK);
	tac_instruction_t instruction = {.opcode = TAC_FUNCTION_BEGIN,
	                                  .name = "factorialExample",
	                                  .is_entrypoint = true};
	assert_int_equal(tac_append(code, &instruction), TAC_OK);
	instruction = (tac_instruction_t){.opcode = TAC_RETURN,
	                                  .src1 = {.kind = TAC_OPERAND_UNIT, .type = SYMT_TYPE_UNIT}};
	assert_int_equal(tac_append(code, &instruction), TAC_OK);
	assert_int_equal(codegen_run(output, gen, code), CDG_OK);
	assert_int_equal(fflush(output), 0);
	assert_int_equal(fseek(output, 0, SEEK_SET), 0);
	char text[8192];
	size_t length = fread(text, 1, sizeof(text) - 1, output);
	text[length] = '\0';
	assert_non_null(strstr(text, "CALL _factorialExample\nEXIT int@0\n"));
	assert_non_null(strstr(text, "LABEL _print\n"));
	assert_non_null(strstr(text, "LABEL _subStr\n"));
	assert_non_null(strstr(text, "GT LF@bad LF@return int@2147483647\n"));
	assert_non_null(strstr(text, "LT LF@bad LF@return int@-2147483648\n"));
	assert_non_null(strstr(text, "GT LF@greater LF@value float@0x1p+52\n"));
	assert_non_null(strstr(text, "LABEL $floor_end\n"));
	assert_non_null(strstr(text, "MOVE LF@return nil@nil\nPOPFRAME\nRETURN\n"));
	tac_destroy(&code);
	assert_int_equal(codegen_free(&gen), CDG_OK);
	assert_int_equal(funct_free(functions), SYMTSTAT_OK);
	assert_int_equal(fclose(output), 0);
}

/**
 * @brief Declares the loop variable before its label.
 */
static void test_loop_variable_before_label(void **state) {
	(void)state;
	funct_t *functions = NULL;
	codegen_t *gen = NULL;
	tac_t *code = NULL;
	FILE *output = tmpfile();
	assert_non_null(output);
	assert_int_equal(funct_init(&functions), SYMTSTAT_OK);
	assert_int_equal(codegen_init(&gen, functions), CDG_OK);
	assert_int_equal(tac_create(&code), TAC_OK);
	tac_instruction_t instruction = {.opcode = TAC_FUNCTION_BEGIN, .name = "main"};
	assert_int_equal(tac_append(code, &instruction), TAC_OK);
	instruction = (tac_instruction_t){.opcode = TAC_LABEL, .label = 8};
	assert_int_equal(tac_append(code, &instruction), TAC_OK);
	instruction = (tac_instruction_t){.opcode = TAC_MOVE,
	                                  .dst = {.kind = TAC_OPERAND_TEMP, .type = SYMT_TYPE_INTEGER, .value.id = 4},
	                                  .src1 = {.kind = TAC_OPERAND_INTEGER, .type = SYMT_TYPE_INTEGER, .value.integer = 1}};
	assert_int_equal(tac_append(code, &instruction), TAC_OK);
	instruction = (tac_instruction_t){.opcode = TAC_JUMP, .label = 8};
	assert_int_equal(tac_append(code, &instruction), TAC_OK);
	instruction = (tac_instruction_t){.opcode = TAC_FUNCTION_END, .name = "main"};
	assert_int_equal(tac_append(code, &instruction), TAC_OK);
	assert_int_equal(codegen_run(output, gen, code), CDG_OK);
	assert_int_equal(fflush(output), 0);
	assert_int_equal(fseek(output, 0, SEEK_SET), 0);
	char text[8192];
	size_t length = fread(text, 1, sizeof(text) - 1, output);
	text[length] = '\0';
	const char *declaration = strstr(text, "DEFVAR LF@t0\n");
	const char *loop = strstr(text, "LABEL &8\n");
	assert_non_null(declaration);
	assert_non_null(loop);
	assert_true(declaration < loop);
	assert_null(strstr(loop, "DEFVAR LF@t0\n"));
	tac_destroy(&code);
	assert_int_equal(codegen_free(&gen), CDG_OK);
	assert_int_equal(funct_free(functions), SYMTSTAT_OK);
	assert_int_equal(fclose(output), 0);
}

/**
 * @brief Wraps integer addition to the signed 32-bit range.
 */
static void test_int32_overflow(void **state) {
	(void)state;
	funct_t *functions = NULL;
	codegen_t *gen = NULL;
	tac_t *code = NULL;
	FILE *output = tmpfile();
	assert_non_null(output);
	assert_int_equal(funct_init(&functions), SYMTSTAT_OK);
	assert_int_equal(codegen_init(&gen, functions), CDG_OK);
	assert_int_equal(tac_create(&code), TAC_OK);
	tac_instruction_t instruction = {.opcode = TAC_FUNCTION_BEGIN, .name = "main"};
	assert_int_equal(tac_append(code, &instruction), TAC_OK);
	instruction = (tac_instruction_t){.opcode = TAC_ADD,
	                                  .dst = {.kind = TAC_OPERAND_TEMP, .type = SYMT_TYPE_INTEGER, .value.id = 1},
	                                  .src1 = {.kind = TAC_OPERAND_INTEGER, .type = SYMT_TYPE_INTEGER, .value.integer = INT32_MAX},
	                                  .src2 = {.kind = TAC_OPERAND_INTEGER, .type = SYMT_TYPE_INTEGER, .value.integer = 1}};
	assert_int_equal(tac_append(code, &instruction), TAC_OK);
	instruction = (tac_instruction_t){.opcode = TAC_RETURN,
	                                  .src1 = {.kind = TAC_OPERAND_UNIT, .type = SYMT_TYPE_UNIT}};
	assert_int_equal(tac_append(code, &instruction), TAC_OK);
	instruction = (tac_instruction_t){.opcode = TAC_FUNCTION_END, .name = "main"};
	assert_int_equal(tac_append(code, &instruction), TAC_OK);
	assert_int_equal(codegen_run(output, gen, code), CDG_OK);
	assert_int_equal(fflush(output), 0);
	assert_int_equal(fseek(output, 0, SEEK_SET), 0);
	char text[10000];
	size_t length = fread(text, 1, sizeof(text) - 1, output);
	text[length] = '\0';
	assert_non_null(strstr(text, "ADD LF@t0 int@2147483647 int@1\n"));
	assert_non_null(strstr(text, "IDIV LF@wrapq LF@t0 int@4294967296\n"));
	assert_non_null(strstr(text, "SUB LF@t0 LF@t0 int@4294967296\n"));
	tac_destroy(&code);
	assert_int_equal(codegen_free(&gen), CDG_OK);
	assert_int_equal(funct_free(functions), SYMTSTAT_OK);
	assert_int_equal(fclose(output), 0);
}

/**
 * @brief Converts the Int operand before mixed numeric addition.
 */
static void test_mixed_numeric_add(void **state) {
	(void)state;
	funct_t *functions = NULL;
	codegen_t *gen = NULL;
	tac_t *code = NULL;
	FILE *output = tmpfile();
	assert_non_null(output);
	assert_int_equal(funct_init(&functions), SYMTSTAT_OK);
	assert_int_equal(codegen_init(&gen, functions), CDG_OK);
	assert_int_equal(tac_create(&code), TAC_OK);
	tac_instruction_t instruction = {.opcode = TAC_FUNCTION_BEGIN, .name = "main"};
	assert_int_equal(tac_append(code, &instruction), TAC_OK);
	instruction = (tac_instruction_t){.opcode = TAC_ADD,
	                                  .dst = {.kind = TAC_OPERAND_TEMP, .type = SYMT_TYPE_DOUBLE, .value.id = 1},
	                                  .src1 = {.kind = TAC_OPERAND_INTEGER, .type = SYMT_TYPE_INTEGER, .value.integer = 1},
	                                  .src2 = {.kind = TAC_OPERAND_DOUBLE, .type = SYMT_TYPE_DOUBLE, .value.decimal = 0.5}};
	assert_int_equal(tac_append(code, &instruction), TAC_OK);
	instruction = (tac_instruction_t){.opcode = TAC_FUNCTION_END, .name = "main"};
	assert_int_equal(tac_append(code, &instruction), TAC_OK);
	assert_int_equal(codegen_run(output, gen, code), CDG_OK);
	assert_int_equal(fflush(output), 0);
	assert_int_equal(fseek(output, 0, SEEK_SET), 0);
	char text[10000];
	size_t length = fread(text, 1, sizeof(text) - 1, output);
	text[length] = '\0';
	assert_non_null(strstr(text, "INT2FLOAT LF@cast1 int@1\n"));
	assert_non_null(strstr(text, "ADD LF@t0 LF@cast1 float@0x1p-1\n"));
	tac_destroy(&code);
	assert_int_equal(codegen_free(&gen), CDG_OK);
	assert_int_equal(funct_free(functions), SYMTSTAT_OK);
	assert_int_equal(fclose(output), 0);
}

/**
 * @brief Generates CONCAT when adding two strings.
 */
static void test_string_add(void **state) {
	(void)state;
	funct_t *functions = NULL;
	codegen_t *gen = NULL;
	tac_t *code = NULL;
	FILE *output = tmpfile();
	assert_non_null(output);
	assert_int_equal(funct_init(&functions), SYMTSTAT_OK);
	assert_int_equal(codegen_init(&gen, functions), CDG_OK);
	assert_int_equal(tac_create(&code), TAC_OK);

	tac_instruction_t instruction = {.opcode = TAC_FUNCTION_BEGIN, .name = "main"};
	assert_int_equal(tac_append(code, &instruction), TAC_OK);
	instruction = (tac_instruction_t){.opcode = TAC_ADD,
	                                  .dst = {.kind = TAC_OPERAND_TEMP,
	                                          .type = SYMT_TYPE_STRING,
	                                          .value.id = 1},
	                                  .src1 = {.kind = TAC_OPERAND_STRING,
	                                           .type = SYMT_TYPE_STRING,
	                                           .value.string = "left"},
	                                  .src2 = {.kind = TAC_OPERAND_STRING,
	                                           .type = SYMT_TYPE_STRING,
	                                           .value.string = "right"}};
	assert_int_equal(tac_append(code, &instruction), TAC_OK);
	instruction = (tac_instruction_t){.opcode = TAC_FUNCTION_END, .name = "main"};
	assert_int_equal(tac_append(code, &instruction), TAC_OK);

	assert_int_equal(codegen_run(output, gen, code), CDG_OK);
	assert_int_equal(fflush(output), 0);
	assert_int_equal(fseek(output, 0, SEEK_SET), 0);
	char text[8192];
	size_t length = fread(text, 1, sizeof(text) - 1, output);
	text[length] = '\0';
	assert_non_null(strstr(text, "CONCAT LF@t0 string@left string@right\n"));
	assert_null(strstr(text, "ADD LF@t0 string@left string@right\n"));

	tac_destroy(&code);
	assert_int_equal(codegen_free(&gen), CDG_OK);
	assert_int_equal(funct_free(functions), SYMTSTAT_OK);
	assert_int_equal(fclose(output), 0);
}

/**
 * @brief Uses the same label for the nonzero check and its target.
 */
static void test_nonzero_check_label(void **state) {
	(void)state;
	funct_t *functions = NULL;
	codegen_t *gen = NULL;
	tac_t *code = NULL;
	FILE *output = tmpfile();
	assert_non_null(output);
	assert_int_equal(funct_init(&functions), SYMTSTAT_OK);
	assert_int_equal(codegen_init(&gen, functions), CDG_OK);
	assert_int_equal(tac_create(&code), TAC_OK);

	tac_instruction_t instruction = {.opcode = TAC_FUNCTION_BEGIN, .name = "main"};
	assert_int_equal(tac_append(code, &instruction), TAC_OK);
	instruction = (tac_instruction_t){.opcode = TAC_CHECK_NONZERO,
	                                  .src1 = {.kind = TAC_OPERAND_INTEGER,
	                                           .type = SYMT_TYPE_INTEGER,
	                                           .value.integer = 1}};
	assert_int_equal(tac_append(code, &instruction), TAC_OK);
	instruction = (tac_instruction_t){.opcode = TAC_FUNCTION_END, .name = "main"};
	assert_int_equal(tac_append(code, &instruction), TAC_OK);

	assert_int_equal(codegen_run(output, gen, code), CDG_OK);
	assert_int_equal(fflush(output), 0);
	assert_int_equal(fseek(output, 0, SEEK_SET), 0);
	char text[8192];
	size_t length = fread(text, 1, sizeof(text) - 1, output);
	text[length] = '\0';
	assert_non_null(strstr(text, "JUMPIFNEQ &i1 int@1 int@0\nEXIT int@40\nLABEL &i1\n"));

	tac_destroy(&code);
	assert_int_equal(codegen_free(&gen), CDG_OK);
	assert_int_equal(funct_free(functions), SYMTSTAT_OK);
	assert_int_equal(fclose(output), 0);
}

int main(void) {
	int test_results = 0;
	const char filename[] = "test_codegen.out";
	outfile = fopen(filename, "w");
	if (outfile == NULL) {
		fprintf(stderr, "Error: failed to open test output file\n");
		return 1;
	}

	/* Tests begin here */
	struct CMUnitTest test_codegen_interface[] = {
	    cmocka_unit_test(test_codegen_alloc_interface),
	    cmocka_unit_test(test_codegen_free_interface),
	    cmocka_unit_test(test_codegen_run_interface),
	};
	EXEC_TEST_CATEGORY("Codegen public interface", test_codegen_interface);

	struct CMUnitTest test_codegen_param_stack[] = {
	    cmocka_unit_test(test_cdg_param_arr_allocation_basic),
	    cmocka_unit_test(test_cdg_param_arr_realloc_increases_size),
	    cmocka_unit_test(test_cdg_param_arr_append_basic),
	    cmocka_unit_test(test_cdg_param_arr_append_to_null),
	    cmocka_unit_test(test_cdg_param_arr_append_resize_is_geometric_series),
	    cmocka_unit_test(test_cdg_param_arr_clear),
	};
	EXEC_TEST_CATEGORY("Param stack functions", test_codegen_param_stack);

	struct CMUnitTest test_codegen_printing[] = {
	    cmocka_unit_test(test_cdg_print_literal),
	    cmocka_unit_test(test_cdg_print_temp_operand_writes_address),
	    cmocka_unit_test(test_cdg_print_temp_operands_different_namespace),
	    cmocka_unit_test(test_cdg_print_symbol_in_temp_frame),
	    cmocka_unit_test(test_cdg_print_reuses_existing_id),
	    cmocka_unit_test(test_cdg_print_combined_types_unique_id),
	    cmocka_unit_test(test_cdg_print_defvar_writes_address_if_undef),
	    cmocka_unit_test(test_cdg_print_defvar_skips_already_defined),
	    cmocka_unit_test(test_cdg_print_defvar_writes_internal_operand),
	};
	EXEC_TEST_CATEGORY("Codegen print functions", test_codegen_printing);

	struct CMUnitTest test_codegen_simple[] = {
	    cmocka_unit_test(test_cdg_run_move_literal_to_temp_several),
	    cmocka_unit_test(test_cdg_run_move_var_to_var),
	    cmocka_unit_test(test_cdg_run_simple_binary_instructions),
	    cmocka_unit_test(test_cdg_run_nonstrict_inequalities),
	    cmocka_unit_test(test_cdg_run_negative_precomputes_literal),
	    cmocka_unit_test(test_cdg_run_negative_on_symbol),
	    cmocka_unit_test(test_cdg_run_float_division),
	    cmocka_unit_test(test_cdg_run_integer_division),
	};
	EXEC_TEST_CATEGORY("Codegen run simple", test_codegen_simple);

	struct CMUnitTest test_function_generation[] = {
	    cmocka_unit_test(test_cdg_run_single_function_definition),
	    cmocka_unit_test(test_cdg_run_function_definition_single_param),
	};
	EXEC_TEST_CATEGORY("Codegen function generation", test_function_generation);

	struct CMUnitTest test_codegen_complex[] = {
	    cmocka_unit_test(test_cdg_generate_complex_branched),
	    cmocka_unit_test(test_not_and_not_equal),
	    cmocka_unit_test(test_entrypoint_and_builtins),
	    cmocka_unit_test(test_loop_variable_before_label),
	    cmocka_unit_test(test_int32_overflow),
	    cmocka_unit_test(test_mixed_numeric_add),
	    cmocka_unit_test(test_string_add),
	    cmocka_unit_test(test_nonzero_check_label),
	};
	EXEC_TEST_CATEGORY("Codegen complex code generation tests", test_codegen_complex);

	fclose(outfile);

	/* Print outfile data & cleanup */
	if (true)
	    return test_results;
	FILE *print_file = fopen(filename, "r");
	fprintf(stderr, "\nContents of test_codegen.out:");

	char buffer[256];
	while (fgets(buffer, sizeof(buffer), print_file) != NULL)
		printf("%s", buffer);

	if (fclose(print_file) == EOF) {
		test_results = 1;
		fprintf(stderr, "Error: failed to close test output file\n");
	}
	return test_results;
}
