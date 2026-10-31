CC				:= gcc
# These will be needed fot github actions, because the ubuntu there has the old versions of CT and CF
CLANG_TIDY		:= clang-tidy
CLANG_FORMAT	:= clang-format

CFLAGS			:= -std=gnu23 -Wall -Wextra -Wpedantic -g #-fsanitize=address
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

# COLORZ <3
YELLOW = \033[33m
GREEN = \033[32m
RESET = \033[0m


.PHONY: tidy tidy-fix tidy-all format format-check check fix test clean $(TEST_RUNS)


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