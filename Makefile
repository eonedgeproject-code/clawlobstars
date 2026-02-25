# ============================================================
# ClawLobstars AI Agent Framework
# Makefile
# ============================================================

CC       ?= gcc
CFLAGS   := -Wall -Wextra -Wpedantic -std=c99 -D_POSIX_C_SOURCE=199309L -Isrc/include
LDFLAGS  := -lrt -lpthread -lm

# Optimization
OPT      ?= O2
CFLAGS   += -$(OPT)

# Debug build
ifdef DEBUG
CFLAGS   += -g -DCLS_DEBUG
endif

# Directories
SRC_DIR  := src
BUILD_DIR := build
BIN_DIR  := $(BUILD_DIR)/bin
OBJ_DIR  := $(BUILD_DIR)/obj

# Source files
SRCS     := $(SRC_DIR)/core/cls_agent.c \
            $(SRC_DIR)/memory/cls_memory.c \
            $(SRC_DIR)/perception/cls_perception.c \
            $(SRC_DIR)/cognitive/cls_cognitive.c \
            $(SRC_DIR)/planning/cls_planning.c \
            $(SRC_DIR)/action/cls_action.c \
            $(SRC_DIR)/knowledge/cls_knowledge.c \
            $(SRC_DIR)/comm/cls_comm.c \
            $(SRC_DIR)/multiagent/cls_multiagent.c \
            $(SRC_DIR)/security/cls_security.c \
            $(SRC_DIR)/training/cls_training.c \
            $(SRC_DIR)/resource/cls_resource.c

OBJS     := $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
DEPS     := $(OBJS:.o=.d)

# Library output
LIB_NAME := libclawlobstars
STATIC_LIB := $(BUILD_DIR)/$(LIB_NAME).a

# Example binary
EXAMPLE_SRC := examples/main.c
EXAMPLE_BIN := $(BIN_DIR)/cls_example

# ============================================================
# Targets
# ============================================================

.PHONY: all build clean lib example test help

all: build

build: lib example
	@echo ""
	@echo "  ╔═══════════════════════════════════════════╗"
	@echo "  ║  CLAWLOBSTARS v0.1.0 — Build Complete    ║"
	@echo "  ║  Library: $(STATIC_LIB)                  ║"
	@echo "  ║  Example: $(EXAMPLE_BIN)                 ║"
	@echo "  ╚═══════════════════════════════════════════╝"
	@echo ""

lib: $(STATIC_LIB)
	@echo "[LIB] $(STATIC_LIB)"

$(STATIC_LIB): $(OBJS)
	@mkdir -p $(BUILD_DIR)
	ar rcs $@ $^

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

example: $(EXAMPLE_BIN)
	@echo "[BIN] $(EXAMPLE_BIN)"

$(EXAMPLE_BIN): $(EXAMPLE_SRC) $(STATIC_LIB)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $< -L$(BUILD_DIR) -lclawlobstars $(LDFLAGS) -o $@

clean:
	rm -rf $(BUILD_DIR)
	@echo "[CLEAN] Build artifacts removed"

help:
	@echo ""
	@echo "  ClawLobstars Build System"
	@echo "  ========================="
	@echo ""
	@echo "  make build       Build library + example"
	@echo "  make lib         Build static library only"
	@echo "  make example     Build example binary"
	@echo "  make clean       Remove build artifacts"
	@echo "  make DEBUG=1     Build with debug symbols"
	@echo "  make OPT=O3      Build with O3 optimization"
	@echo ""

# Cross-compilation targets
.PHONY: arm riscv

arm:
	$(MAKE) CC=arm-none-eabi-gcc TARGET=arm-cortex-m4

riscv:
	$(MAKE) CC=riscv32-unknown-elf-gcc TARGET=riscv32

-include $(DEPS)
