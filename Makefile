# disable built-in rules
MAKEFLAGS += --no-builtin-rules
.SUFFIXES:

# recursive wildcard 
rwildcard = $(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2) $(filter $(subst *,%,$2),$d))
# remove duplicates from list
uniq      = $(if $1,$(firstword $1) $(call uniq,$(filter-out $(firstword $1),$1)))

# Flags that are conditional on whether or not this is a release
OFLAGS = $(if $(RELEASE),-O3,-Og -gdwarf-4)
DEFINES = -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE 
DEFINES += $(if $(RELEASE),-DNDEBUG,-DDEBUG)
BIN_ROOT = out
BIN_DIR = $(if $(RELEASE),$(BIN_ROOT)/$(CC)/release/,$(BIN_ROOT)/$(CC)/debug/)
LDFLAGS = $(if $(RELEASE),-s,)

COMPILE_COMMANDS = compile_commands.json
INCLUDE_DIR      = include
OBJ_DIR          = src
TEST_DIR         = test
CMD_DIR          = cmd
EMBED_DIR        = embed

OBJ_OUT_DIR  = $(BIN_DIR)$(OBJ_DIR)
TEST_OUT_DIR = $(BIN_DIR)$(TEST_DIR)
CMD_OUT_DIR  = $(BIN_DIR)$(CMD_DIR)
EMBED_OUT_DIR = $(BIN_ROOT)/$(EMBED_DIR)

OBJ_SRC  = $(call rwildcard,$(OBJ_DIR),*.c)
TEST_SRC = $(call rwildcard,$(TEST_DIR),*.c)
CMD_SRC  = $(call rwildcard,$(CMD_DIR),*.c)

# Only look for embed files if the embed directory exists
ifeq ($(wildcard $(EMBED_DIR)),$(EMBED_DIR))
	EMBED_SRC = $(shell find $(EMBED_DIR) -type f)
endif

SRC = $(OBJ_SRC) $(TEST_SRC) $(CMD_SRC)
OBJECTS = $(patsubst %, $(BIN_DIR)%, $(SRC:.c=.o))

TEST_OUT = $(patsubst $(TEST_DIR)/%, $(TEST_OUT_DIR)/%, $(TEST_SRC:.c=))
CMD_OUT  = $(patsubst $(CMD_DIR)/%, $(CMD_OUT_DIR)/%, $(CMD_SRC:.c=))
BINARIES = $(TEST_OUT) $(CMD_OUT)

EMBED_OUT = $(patsubst $(EMBED_DIR)/%, $(EMBED_OUT_DIR)/%, $(EMBED_SRC))

# This target exists mostly as a hack to make incremental builds that only run tests where the inputs changed
TEST_RESULT = $(TEST_OUT:=.log)
VALGRIND_RESULT = $(TEST_OUT:=.valgrind)
VALGRIND_FLAGS = --error-exitcode=1 -s --leak-check=full --track-origins=yes --show-leak-kinds=all --quiet

MMD_FILES = $(OBJECTS:.o=.o.d)
DIRECTORIES = $(call uniq,$(dir $(OBJECTS) $(EMBED_OUT)))

CFLAGS += -std=c1x -pedantic -Wall -Wextra -Werror -pipe $(DEFINES) -I$(INCLUDE_DIR) -I$(EMBED_OUT_DIR) $(OFLAGS)
MMD_FLAGS = -MMD -MF $@.d

.PHONY: all
all: $(OBJECTS) $(BINARIES)

$(DIRECTORIES):
	@mkdir -p $(DIRECTORIES)

.PRECIOUS: $(EMBED_OUT)%
$(EMBED_OUT_DIR)/%: $(EMBED_DIR)/% | $(DIRECTORIES)
	xxd -i < $< > $@

# The .o file of binary outputs are implicit dependencies and will be removed unless precious
.PRECIOUS: $(BIN_DIR)%.o
$(BIN_DIR)%.o: %.c | $(EMBED_OUT) $(DIRECTORIES)
	$(CC) $(CFLAGS) $(MMD_FLAGS) -c $< \
		-o $@

$(BIN_DIR)%: $(BIN_DIR)%.o | $(DIRECTORIES)
	$(CC) $(CFLAGS) $(filter %.o,$^) $(LDFLAGS) \
		-o $@

# Delete the output logs from tests
.PHONY: clean-valgrind
clean-valgrind:
	rm -f	$(VALGRIND_RESULT) $(VALGRIND_RESULT:=.error)


.PHONY: clean-test
clean-test:
	rm -f $(TEST_RESULT) $(TEST_RESULT:=.error)

# Delete intermediate files so only the desired build artifacts remain
.PHONY: intermediate-clean
intermediate-clean: clean-test clean-valgrind
	rm -f $(EMBED_OUT) $(OBJECTS) $(MMD_FILES)

.PHONY: clean
clean: intermediate-clean
	rm -f $(BINARIES)

# distclean and mostlyclean don't really make sense yet, but are included for completeness
.PHONY: distclean
distclean: clean

.PHONY: mostlyclean
mostlyclean: clean

.PHONY: maintainer-clean
maintainer-clean: clean
	rm -f $(COMPILE_COMMANDS) 
	rm -rf out

$(COMPILE_COMMANDS): Makefile $(OBJECTS) $(BINARIES)
	bear -- $(MAKE) all -j$(nproc) -B

# Run all tests
.PHONY: test
test: clean-test incremental-test

# Run all valgrinds
.PHONY: valgrind
valgrind: clean-valgrind incremental-valgrind

# Run tests if input changed
$(TEST_OUT_DIR)%.log: $(TEST_OUT_DIR)%
	@(                                                      \
		./$< > $@.error 2>&1;                                 \
		RET=$$?;                                              \
		printf "\033[1;30;44m====== $< ======\033[0m\n";      \
		cat $@.error;                                         \
		if test $$RET -eq 0; then                             \
		printf "\033[1;30;46m====== PASS ======\033[0m\n";    \
		mv $@.error $@;                                       \
		else                                                  \
		printf "\033[1;30;41m====== FAIL ======\033[0m\n";    \
		exit $$RET;                                           \
		fi                                                    \
		)

# Run tests where input changed
.PHONY: incremental-test
incremental-test: $(TEST_RESULT)

# Generate valgrind report if input changed
$(TEST_OUT_DIR)%.valgrind: $(TEST_OUT_DIR)%
	@(                                                               \
		valgrind $(VALGRIND_FLAGS) ./$< > $@.error 2>&1;               \
		RET=$$?;                                                       \
		printf "\033[1;30;44m====== VALGRIND $< ======\033[0m\n";      \
		cat $@.error;                                                  \
		if test $$RET -eq 0; then                                      \
		printf "\033[1;30;46m====== VALGRIND DONE ======\033[0m\n";    \
		mv $@.error $@;                                                \
		else                                                           \
		printf "\033[1;30;41m====== VALGRIND ERROR ======\033[0m\n";   \
		exit $$RET;                                                    \
		fi                                                             \
		)

# Generate all missing valgrind reports
.PHONY: incremental-valgrind
incremental-valgrind: $(VALGRIND_RESULT)

# Include object dependencies generated by -MMD -MF $@.d
-include $(MMD_FILES)

# $1: source file
# $2: output file
# awk: for each line starting with '// link', print each following argument, separated by whitespace, prepended with the obj out dir
define LINK_OBJECTS
DEPS := $(shell awk '$$0 ~ /^\/\/ *link/ { for (i = 3; i <= NF; i++) print "$$(OBJ_OUT_DIR)/" $$i  }' $(1))
$(2): $$(DEPS)
endef

$(foreach target,$(TEST_SRC) $(CMD_SRC), $(eval $(call \
	LINK_OBJECTS,\
	$(target),\
	$(patsubst %, $(BIN_DIR)%, $(target:.c=)))))

.PHONY: test-all gcc-test clang-test gcc-valgrind clang-valgrind
gcc-test:
	CC=gcc             $(MAKE) incremental-test
	CC=gcc   RELEASE=1 $(MAKE) incremental-test
clang-test:
	CC=clang           $(MAKE) incremental-test
	CC=clang RELEASE=1 $(MAKE) incremental-test
gcc-valgrind:
	CC=gcc             $(MAKE) incremental-valgrind
	CC=gcc   RELEASE=1 $(MAKE) incremental-valgrind
clang-valgrind:
	CC=clang           $(MAKE) incremental-valgrind
	CC=clang RELEASE=1 $(MAKE) incremental-valgrind

test-all: gcc-test clang-test gcc-valgrind clang-valgrind
