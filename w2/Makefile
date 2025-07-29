# 多架构构建 Makefile
# 支持: make x86, make arm, make all, make clean
APP_NAME = hello
# 工具定义
RM = rm -rf
MKDIR = mkdir -p
FIND = find

# 目录定义
SRC_DIR = src
OBJ_DIR = build
BIN_DIR = bin
INCLUDE_DIR = include

# 自动获取源文件
SRCS = $(shell $(FIND) $(SRC_DIR) -name '*.c')
OBJ_DIRS = $(OBJ_DIR)/x86 $(OBJ_DIR)/arm
BIN_DIRS = $(BIN_DIR)/x86 $(BIN_DIR)/arm

# 编译器定义
X86_CC = gcc
ARM_CC = arm-linux-gnueabihf-gcc

# 编译选项
COMPILE_FLAGS = -Wall -Wextra -I$(INCLUDE_DIR)
LINK_FLAGS = -lm

# 默认目标
all: x86 arm

# 创建必要的目录
$(OBJ_DIRS) $(BIN_DIRS):
	@$(MKDIR) $@

# x86 目标
x86: CC = $(X86_CC)
x86: | $(OBJ_DIR)/x86 $(BIN_DIR)/x86
x86: $(BIN_DIR)/x86/$(APP_NAME)_x86

# ARM 目标
arm: CC = $(ARM_CC)
arm: | $(OBJ_DIR)/arm $(BIN_DIR)/arm
arm: $(BIN_DIR)/arm/$(APP_NAME)_arm

# x86 可执行文件
$(BIN_DIR)/x86/$(APP_NAME)_x86: $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/x86/%.o,$(SRCS))
	$(CC) $^ -o $@ $(LINK_FLAGS)

# ARM 可执行文件
$(BIN_DIR)/arm/$(APP_NAME)_arm: $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/arm/%.o,$(SRCS))
	$(CC) $^ -o $@ $(LINK_FLAGS)

# 编译规则 (x86)
$(OBJ_DIR)/x86/%.o: $(SRC_DIR)/%.c
	@$(MKDIR) $(@D)
	$(CC) $(COMPILE_FLAGS) -c $< -o $@

# 编译规则 (ARM)
$(OBJ_DIR)/arm/%.o: $(SRC_DIR)/%.c
	@$(MKDIR) $(@D)
	$(CC) $(COMPILE_FLAGS) -c $< -o $@

# 清理所有构建产物
clean:
	$(RM) $(OBJ_DIR) $(BIN_DIR)

# 伪目标声明
.PHONY: all x86 arm clean
