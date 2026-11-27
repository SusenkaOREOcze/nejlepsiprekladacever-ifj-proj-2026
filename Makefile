CC				:= gcc
# These will be needed fot github actions, because the ubuntu there has the old versions of CT and CF
CLANG_TIDY		:= clang-tidy
CLANG_FORMAT	:= clang-format

CFLAGS			:= -std=gnu23 -Wall -Wextra -g -fsanitize=address
CPPFLAGS		:= -Isrc/include #preprocessor flags, not C++, thoose are CXXFLAGS :/

# Directories for tests and CMocka.
TEST_DIR		:= src/tests
CMOCKA_DIR		:= $(TEST_DIR)/cmocka
TEST_BIN_DIR	:= $(TEST_DIR)/bin

# Source files and headers.
# Let's make it fancy, we sort and remove duplicates from the lists.
SRC			:= $(sort $(wildcard src/*.c))
HEADERS		:= $(sort $(wildcard src/include/*.h))

# No main() allowed -- tests provide their own main().
# this will change based on the projects main file
PROJECT_SRC	:= $(filter-out src/main.c,$(SRC))

# Get all test_*.c files.
TEST_SRC	:= $(sort $(wildcard $(TEST_DIR)/test_*.c))

# Example:
#   src/tests/test_lexer.c -> lexer
TEST_NAMES	:= $(patsubst $(TEST_DIR)/test_%.c,%,$(TEST_SRC))

TEST_CPPFLAGS := $(CPPFLAGS) -I$(CMOCKA_DIR)

# CMocka ._.
# it is uunder APACHE 2.0 license, so we can use it in our project - more in README.md
CMOCKA_SRC		:= $(CMOCKA_DIR)/cmocka.c
CMOCKA_HEADERS	:= $(sort $(wildcard $(CMOCKA_DIR)/*.h))
CMOCKA_OBJ		:= $(TEST_BIN_DIR)/cmocka.o
CMOCKA_LIB		:= $(TEST_BIN_DIR)/libcmocka.a

# Example:
#   lexer -> src/tests/bin/test_lexer
TEST_BINS := $(addprefix $(TEST_BIN_DIR)/test_,$(TEST_NAMES)))

# Example:
#   lexer -> run-lexer
TEST_RUNS := $(addprefix run-,$(TEST_NAMES))

# Submission archive. Sources are flattened.
SUBMISSION_SOURCES := $(sort $(wildcard src/*.c src/include/*.h))
SUBMISSION_STAGE := build/submission-package
SUBMISSION_ZIP := build/IFJ26_submission.zip
SUBMISSION_ZIP_ABS := $(abspath $(SUBMISSION_ZIP))
SUBMISSION_DOC := $(wildcard docs/dokumentace.pdf)
SUBMISSION_EXTENSIONS ?= BOOLTHEN CONTROL FUNEXP VALMEM
SUBMISSION_REQUIRE_METADATA ?= 0

# COLORZ <3
YELLOW = \033[33m
GREEN = \033[32m
RESET = \033[0m


.PHONY: tidy tidy-fix tidy-all format format-check check fix test clean zip zip-end $(TEST_RUNS)


# =============
# CLANG-TIDY
# =============

tidy:
	$(CLANG_TIDY) --use-color $(SRC) -- $(CFLAGS) $(CPPFLAGS)

tidy-fix:
	$(CLANG_TIDY) --use-color --fix $(SRC) -- $(CFLAGS) $(CPPFLAGS)

tidy-all:
	$(CLANG_TIDY) --use-color --checks=* $(SRC) -- $(CFLAGS) $(CPPFLAGS)


# =============
# CLANG-FORMAT
# =============

format:
	$(CLANG_FORMAT) -i $(SRC) $(HEADERS)

format-check:
	$(CLANG_FORMAT) --dry-run $(SRC) $(HEADERS)


# =============
# CHECK / FIX
# =============

check:
	@printf "$(YELLOW)Running clang-format check...$(RESET)\n"
	$(MAKE) format-check
	@printf "$(YELLOW)Running clang-tidy...$(RESET)\n"
	$(MAKE) tidy
	@printf "$(GREEN)All checks finished.$(RESET)\n"

fix:
	@printf "$(YELLOW)Applying clang-tidy fixes...$(RESET)\n"
	$(MAKE) tidy-fix
	@printf "$(YELLOW)Running clang-format...$(RESET)\n"
	$(MAKE) format
	@printf "$(GREEN)Fixes finished.$(RESET)\n"

# format is run after tidy-fix, so the "FIXED" code is formatted as well.


# =============
# CMOCKA
# =============

# Compile CMocka into an object file.
$(CMOCKA_OBJ): $(CMOCKA_SRC) $(CMOCKA_HEADERS)
	@mkdir -p $(TEST_BIN_DIR)
	@printf "$(YELLOW)Building CMocka...$(RESET)\n"
	@$(CC) $(CFLAGS) -I$(CMOCKA_DIR) -c $(CMOCKA_SRC) -o $(CMOCKA_OBJ)


# Create a static CMocka library.
$(CMOCKA_LIB): $(CMOCKA_OBJ)
	@printf "$(YELLOW)Creating CMocka static library...$(RESET)\n"
	@$(AR) rcs $(CMOCKA_LIB) $(CMOCKA_OBJ)
# expects that ar is installed, on linux it should be

# =============
# TESTS
# =============

# Build and run every test_*.c file.
test: $(TEST_RUNS)
	@printf "$(GREEN)All tests passed.$(RESET)\n"

# Build one executable for each test source file.
#
# Example:
#   src/tests/test_lexer.c -> src/tests/bin/test_lexer
#
# IDK how this thing works rn
# newermind, i remembured now
$(TEST_BIN_DIR)/test_%: \
		$(TEST_DIR)/test_%.c \
		$(PROJECT_SRC) \
		$(HEADERS) \
		$(CMOCKA_LIB)
	@mkdir -p $(TEST_BIN_DIR)
	@printf "$(YELLOW)Building test $*...$(RESET)\n"
	@$(CC) $(CFLAGS) $(TEST_CPPFLAGS) \
		$(filter %.c,$^) \
		$(CMOCKA_LIB) \
		-o $@
# so, for each test_*.c file; build an executable in the bin dir.


# Run each test executable.
#
# Example:
#   run-lexer -> src/tests/bin/test_lexer
#
$(TEST_RUNS): run-%: $(TEST_BIN_DIR)/test_%
	@printf "$(YELLOW)Running test $*...$(RESET)\n"
	@$(TEST_BIN_DIR)/test_$*

# =============
# TESTS /w VALGRIND
# =============

valgrind: $(TEST_RUNS)
	@printf "$(YELLOW)Running tests with Valgrind...$(RESET)\n"
	@for test in $(TEST_NAMES); do \
		printf "$(YELLOW)Running test $$test with Valgrind...$(RESET)\n"; \
		valgrind --leak-check=full --error-exitcode=1 \
			$(TEST_BIN_DIR)/test_$$test; \
	done
	@printf "$(GREEN)All tests passed.$(RESET)\n"

# =============
# CLEAN
# =============

clean:
	@rm -rf $(TEST_BIN_DIR)

# =============
# SUBMISSION ZIP
# =============

zip:
# this is pure hell
	@set -eu; \
	stage_path="$$(realpath -m "$(SUBMISSION_STAGE)")"; \
	case "$$stage_path" in "$$(pwd -P)"/build/*) ;; *) printf '%s\n' 'Submission staging must stay under this project/build/.' >&2; exit 1;; esac; \
	rm -rf "$(SUBMISSION_STAGE)"; \
	mkdir -p "$(SUBMISSION_STAGE)" "$(dir $(SUBMISSION_ZIP))"; \
	cp $(SUBMISSION_SOURCES) "$(SUBMISSION_STAGE)/"; \
	sed -i 's#"include/#"#g' "$(SUBMISSION_STAGE)"/*.[ch]; \
	mkdir -p "$(SUBMISSION_STAGE)/tests/cmocka"; \
	cp $(TEST_DIR)/test_*.c $(TEST_DIR)/ast_builder.h $(TEST_DIR)/*.ifj26 "$(SUBMISSION_STAGE)/tests/"; \
	cp $(CMOCKA_DIR)/*.c $(CMOCKA_DIR)/*.h "$(SUBMISSION_STAGE)/tests/cmocka/"; \
	sed -i 's#"../include/#"../#g; s#src/tests/#tests/#g' \
		"$(SUBMISSION_STAGE)"/tests/test_*.c "$(SUBMISSION_STAGE)"/tests/ast_builder.h; \
	cp Makefile.submission "$(SUBMISSION_STAGE)/Makefile"; \
	for source in main.c lexer.c syntax_anal.c semantic_anal.c ast.c irgen.c tac.c codegen.c; do \
		if [ ! -s "$(SUBMISSION_STAGE)/$$source" ]; then \
			printf 'Missing required source: %s\n' "$$source" >&2; exit 1; \
		fi; \
	done; \
	if [ -s rozdeleni.txt ]; then \
		awk '{ sub(/\r$$/, ""); print }' rozdeleni.txt > "$(SUBMISSION_STAGE)/rozdeleni"; \
		if [ "$(SUBMISSION_REQUIRE_METADATA)" = 1 ]; then \
			awk -F: 'NF != 3 || $$1 !~ /^[0-9]+$$/ || $$2 !~ /^[A-Za-z][A-Za-z0-9_]*$$/ || $$3 !~ /^[0-9]+$$/ { bad = 1 } { total += $$3; count++ } END { if (bad || count == 0 || total != 100) exit 1 }' \
				"$(SUBMISSION_STAGE)/rozdeleni" || { printf '%s\n' 'rozdeleni must contain VUT_ID:FIT_LOGIN:PERCENT lines totalling 100.' >&2; exit 1; }; \
		fi; \
	else \
		if [ "$(SUBMISSION_REQUIRE_METADATA)" = 1 ]; then \
			printf '%s\n' 'Fill rozdeleni.txt before final submission.' >&2; exit 1; \
		fi; \
		: > "$(SUBMISSION_STAGE)/rozdeleni"; \
		printf '%s\n' 'Warning: rozdeleni is empty; archive is a draft.'; \
	fi; \
	printf '%s\n' $(SUBMISSION_EXTENSIONS) > "$(SUBMISSION_STAGE)/rozsireni"; \
	if [ -n "$(SUBMISSION_DOC)" ]; then \
		cp "$(SUBMISSION_DOC)" "$(SUBMISSION_STAGE)/dokumentace.pdf"; \
	else \
		if [ "$(SUBMISSION_REQUIRE_METADATA)" = 1 ]; then \
			printf '%s\n' 'Add docs/dokumentace.pdf before final submission.' >&2; exit 1; \
		fi; \
		printf '%s\n' 'Warning: documentation is missing; archive is a draft.'; \
	fi; \
	$(MAKE) --no-print-directory -C "$(SUBMISSION_STAGE)"; \
	printf 'import ifj26.*\n@main def main(): Unit = ()\n' | "$(SUBMISSION_STAGE)/compiler" > /dev/null; \
	rm -f "$(SUBMISSION_STAGE)/compiler"; \
	rm -f "$(SUBMISSION_ZIP_ABS)"; \
	cd "$(SUBMISSION_STAGE)"; \
	zip -q -r "$(SUBMISSION_ZIP_ABS)" ./*; \
	printf 'Created %s\n' "$(SUBMISSION_ZIP)"

zip-end:
	@test -s rozdeleni.txt || { printf '%s\n' 'Fill rozdeleni.txt before final submission.' >&2; exit 1; }
	@test -s "$(SUBMISSION_DOC)" || { printf '%s\n' 'Add docs/dokumentace.pdf before final submission.' >&2; exit 1; }
	@$(MAKE) --no-print-directory SUBMISSION_REQUIRE_METADATA=1 zip
