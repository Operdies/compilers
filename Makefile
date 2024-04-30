# TODO: Object dependencies must be manually specified in the source files. A more automatic solution would be nice

# recursive wildcard 
rwildcard = $(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2) $(filter $(subst *,%,$2),$d))
# remove duplicates from list
uniq      = $(if $1,$(firstword $1) $(call uniq,$(filter-out $(firstword $1),$1)))

# Flags that are conditional on whether or not this is a release
OFLAGS = $(if $(RELEASE),-O3,-Og -g -rdynamic)
DEFINES = -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE 
DEFINES += $(if $(RELEASE),-DNDEBUG,-DDEBUG)
BIN_DIR = $(if $(RELEASE),out/release,out/debug)
LDFLAGS = $(if $(RELEASE),-s,)

COMPILE_COMMANDS = compile_commands.json
INCLUDE_DIR      = include
OBJ_DIR          = src
TEST_DIR         = test
CMD_DIR          = cmd

OBJ_OUT_DIR  = $(BIN_DIR)/obj
TEST_OUT_DIR = $(BIN_DIR)/test
CMD_OUT_DIR  = $(BIN_DIR)/cmd

OBJ_SRC  = $(call rwildcard,$(OBJ_DIR),*.c)
TEST_SRC = $(call rwildcard,$(TEST_DIR),*.c)
CMD_SRC  = $(call rwildcard,$(CMD_DIR),*.c)

OBJ_OUT  = $(patsubst $(OBJ_DIR)/%,  $(OBJ_OUT_DIR)/%, $(OBJ_SRC:.c=.o))
TEST_OUT = $(patsubst $(TEST_DIR)/%, $(TEST_OUT_DIR)/%, $(TEST_SRC:.c=))
CMD_OUT  = $(patsubst $(CMD_DIR)/%, $(CMD_OUT_DIR)/%, $(CMD_SRC:.c=))

# This target exists mostly as a hack to make incremental builds that only run tests where the inputs changed
TEST_RESULT = $(TEST_OUT:=.log)
VALGRIND_RESULT = $(TEST_OUT:=.valgrind)
VALGRIND_FLAGS = --error-exitcode=1 -s --leak-check=full --track-origins=yes --show-leak-kinds=all

MMD_FILES = $(call rwildcard,$(BIN_DIR),*.d)
DIRECTORIES = $(call uniq,$(dir $(OBJ_OUT) $(TEST_OUT) $(CMD_OUT)))

CFLAGS += -std=c1x -pedantic -Wall -Wextra -Werror $(DEFINES) -I$(INCLUDE_DIR) $(OFLAGS)
MMD_FLAGS = -MMD -MF $@.d

.PHONY: all
all: $(OBJ_OUT) $(TEST_OUT) $(CMD_OUT)

$(DIRECTORIES):
	@mkdir -p $(DIRECTORIES)

# FIXME: these 3 rules are nearly identical.
.PRECIOUS: $(OBJ_OUT_DIR)%.o
$(OBJ_OUT_DIR)%.o: $(OBJ_DIR)%.c | $(DIRECTORIES)
	$(CC) $(CFLAGS) $(MMD_FLAGS) -c -o $@ $<

.PRECIOUS: $(TEST_OUT_DIR)%.o
$(TEST_OUT_DIR)%.o: $(TEST_DIR)%.c | $(DIRECTORIES)
	$(CC) $(CFLAGS) $(MMD_FLAGS) -c -o $@ $<

.PRECIOUS: $(CMD_OUT_DIR)%.o
$(CMD_OUT_DIR)%.o: $(CMD_DIR)%.c | $(DIRECTORIES)
	$(CC) $(CFLAGS) $(MMD_FLAGS) -c -o $@ $<

# FIXME: these 2 rules are nearly identical
$(TEST_OUT_DIR)%: $(TEST_OUT_DIR)%.o | $(DIRECTORIES)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) $(LDFLAGS)
$(CMD_OUT_DIR)%: $(CMD_OUT_DIR)%.o | $(DIRECTORIES)
	$(CC) $(CFLAGS) -o $@ $(filter %.o,$^) $(LDFLAGS)


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
	rm -f $(OBJ_OUT) $(MMD_FILES)

.PHONY: clean
clean: intermediate-clean
	rm -f $(TEST_OUT) $(CMD_OUT)

# distclean and mostlyclean don't really make sense yet, but are included for completeness
.PHONY: distclean
distclean: clean

.PHONY: mostlyclean
mostlyclean: clean

.PHONY: maintainer-clean
maintainer-clean: clean
	rm -f $(COMPILE_COMMANDS) 
	rm -rf out

$(COMPILE_COMMANDS): Makefile $(OBJ_OUT) $(TEST_OUT) $(CMD_OUT)
	bear -- make all -j -B

# Run all tests
.PHONY: test
test: clean-test incremental-test

# Run all valgrinds
.PHONY: valgrind
valgrind: clean-valgrind incremental-valgrind

# Run tests if input changed
$(TEST_OUT_DIR)%.log: $(TEST_OUT_DIR)%
	@(                                                   \
		./$< > $@.error 2>&1;                              \
		RET=$$?;                                           \
		printf "\033[1;30;44m====== $< ======\033[0m\n";   \
		cat $@.error;                                      \
		if test $$RET -eq 0; then                          \
		printf "\033[1;30;46m====== PASS ======\033[0m\n"; \
		mv $@.error $@;                                    \
		else                                               \
		printf "\033[1;31m====== FAIL ======\033[0m\n";    \
		exit $$RET;                                        \
		fi                                                 \
		)

# Run tests where input changed
.PHONY: incremental-test
incremental-test: $(TEST_RESULT)

# Generate valgrind report if input changed
$(TEST_OUT_DIR)%.valgrind: $(TEST_OUT_DIR)%
	@(                                                            \
		valgrind $(VALGRIND_FLAGS) ./$< > $@.error 2>&1;            \
		RET=$$?;                                                    \
		printf "\033[1;30;44m====== VALGRIND $< ======\033[0m\n";   \
		cat $@.error;                                               \
		if test $$RET -eq 0; then                                   \
		printf "\033[1;30;46m====== VALGRIND DONE ======\033[0m\n"; \
		mv $@.error $@;                                             \
		else                                                        \
		printf "\033[1;31m====== VALGRIND ERROR ======\033[0m\n";   \
		exit $$RET;                                                 \
		fi                                                          \
		)

# Generate all missing valgrind reports
.PHONY: incremental-valgrind
incremental-valgrind: $(VALGRIND_RESULT)

.PHONY: release
release: all intermediate-clean

# Include object dependencies generated by -MMD -MF $@.d
-include $(MMD_FILES)

# $1: source file
# $2: output file
# awk: for each line starting with '// link', print each following argument, separated by whitespace, prepended with the obj out dir
define LINK_OBJECTS
DEPS := $(shell awk '$$0 ~ /^\/\/ *link/ { for (i = 3; i <= NF; i++) print "$$(OBJ_OUT_DIR)/" $$i  }' $(1))
$(2): $$(DEPS)
endef

$(foreach test,$(TEST_SRC), $(eval $(call \
	LINK_OBJECTS,\
	$(test),\
	$(patsubst $(TEST_DIR)/%, $(TEST_OUT_DIR)/%, $(test:.c=)))))

$(foreach cmd,$(CMD_SRC), $(eval $(call \
	LINK_OBJECTS,\
	$(cmd),\
	$(patsubst $(CMD_DIR)/%, $(CMD_OUT_DIR)/%, $(cmd:.c=)))))
