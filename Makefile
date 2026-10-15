# Project name (output binary)
TARGET = GuitarLoopPedal

# Sources
CPP_SOURCES = src/main.cpp \
              src/pedal_state.cpp \
              src/hardware.cpp \
              src/metronome.cpp \
              src/controls.cpp \
              src/ui.cpp

# Library locations (relative to this Makefile)
LIBDAISY_DIR = libDaisy
DAISYSP_DIR  = DaisySP

# Daisy core build system
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile
