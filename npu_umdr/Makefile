# Variables
CC := gcc
AR := ar
CFLAGS := -O2 -g -Wall -Wextra -std=c11 -fPIC -Iinclude
LDFLAGS := 

LIB_NAME := libnpuumdr
BUILD_DIR := build
SRC_DIR := src
INC_DIR := include
EXAMPLES_DIR := examples

LIB_OBJS := \
	$(BUILD_DIR)/vfio_helpers.o \
	$(BUILD_DIR)/npu_umdr.o

EXAMPLES := \
	$(BUILD_DIR)/simple_infer

.PHONY: all clean

all: $(BUILD_DIR)/$(LIB_NAME).a $(EXAMPLES)

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/$(LIB_NAME).a: $(LIB_OBJS) | $(BUILD_DIR)
	$(AR) rcs $@ $^

$(BUILD_DIR)/simple_infer: $(EXAMPLES_DIR)/simple_infer.c $(BUILD_DIR)/$(LIB_NAME).a | $(BUILD_DIR)
	$(CC) $(CFLAGS) $< -L$(BUILD_DIR) -lnpuumdr -o $@

clean:
	rm -rf $(BUILD_DIR)