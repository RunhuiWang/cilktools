TOOL_HOME ?= ../..
LIB_DIR ?= $(TOOL_HOME)/lib
INCLUDE_DIR ?= $(TOOL_HOME)/include

TEST = matteo-knapsack

default : $(TEST)

include $(INCLUDE_DIR)/mk.common
include ../test.mk

-include $(TEST).d

