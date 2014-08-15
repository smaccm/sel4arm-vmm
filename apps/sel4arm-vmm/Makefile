#
# Copyright 2014, NICTA
#
# This software may be distributed and modified according to the terms of
# the BSD 2-Clause license. Note that NO WARRANTY is provided.
# See "LICENSE_BSD2.txt" for details.
#
# @TAG(NICTA_BSD)
#

# Targets
TARGETS := sel4arm-vmm.bin

ifeq ($(ARM_HYP),)
$(error sel4arm-vmm can only be build with hypervisor kernel)
endif

ifeq (${COMPONENTS},)
$(error "No compoments setup to build against.")
endif

# Source files required to build the target
CFILES   := $(patsubst $(SOURCE_DIR)/%,%,$(wildcard $(SOURCE_DIR)/src/*.c))
ASMFILES := $(patsubst $(SOURCE_DIR)/%,%,$(wildcard $(SOURCE_DIR)/src/*.S))
OFILES   := archive.o

# Libraries required to build the target
LIBS = muslc sel4 sel4muslcsys sel4vka sel4twinkle \
       platsupport sel4platsupport sel4vspace elf \
       sel4utils sel4simple utils  sel4simple-default cpio \
       sel4arm-vmm

#CFLAGS += -Werror

include $(SEL4_COMMON)/common.mk


${COMPONENTS}:
	false

archive.o: ${COMPONENTS}
	$(Q)mkdir -p $(dir $@)
	${COMMON_PATH}/files_to_obj.sh $@ _cpio_archive $^


