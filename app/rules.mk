LOCAL_DIR := $(GET_LOCAL_DIR)

OBJECTS += \
	$(CURRENT_ROOT)/$(LOCAL_DIR)/main.o \
	$(CURRENT_ROOT)/$(LOCAL_DIR)/halt.o \
	$(CURRENT_ROOT)/$(LOCAL_DIR)/term.o \
	$(CURRENT_ROOT)/$(LOCAL_DIR)/config.o \
	$(CURRENT_ROOT)/$(LOCAL_DIR)/event.o \
	$(CURRENT_ROOT)/$(LOCAL_DIR)/tty.o \
	$(CURRENT_ROOT)/$(LOCAL_DIR)/utils/ll.o \
	$(CURRENT_ROOT)/$(LOCAL_DIR)/utils/misc.o \
	$(CURRENT_ROOT)/$(LOCAL_DIR)/log.o \
	$(CURRENT_ROOT)/$(LOCAL_DIR)/lolcat.o \
	$(CURRENT_ROOT)/$(LOCAL_DIR)/iboot.o \
	$(CURRENT_ROOT)/$(LOCAL_DIR)/seq.o

CFLAGS += -I$(LOCAL_DIR)/include
