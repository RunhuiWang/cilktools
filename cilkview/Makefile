BASENAME ?= $(shell basename $(CURDIR))

LIB_DIR = ../lib
INCLUDE_DIR = ../include

.PHONY : default clean

default :

include $(INCLUDE_DIR)/mk.common
include $(BASENAME).mk

TARGETS = $(LIBCILKVIEW) $(LIBCILKVIEW_PERF)

default : $(TARGETS)

clean : clean$(BASENAME)
