# Copyright (c) Microsoft Corporation. All rights reserved

SUBDIRS := $(shell find . -type f -name Makefile -exec dirname {} \; | grep -v ^\.$)

# Default target: Build all projects
.PHONY: all
all: $(SUBDIRS)

# Rule to go into each subdirectory and run 'make'
$(SUBDIRS):
	@echo "Entering directory '$@'"
	@$(MAKE) -C $@ all

# Clean rule to clean all subdirectories
.PHONY: clean
clean: 
	@for dir in $(SUBDIRS); do \
	    $(MAKE) -C $$dir clean; \
	done

# Phony targets to avoid conflicts with files
.PHONY: $(SUBDIRS)
