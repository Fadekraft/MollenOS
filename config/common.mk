# Makefile with common rules and defines for the system
# Defines paths
#

# Sanitize for architecture
ifndef VALI_ARCH
$(error VALI_ARCH is not set)
endif

# Sanitize for toolchain
ifndef CROSS
$(error CROSS is not set)
endif

# Sanitize for pvs analyser
ifndef VALI_PVS
VALI_PVS=false
endif

# Setup project tools
CC := $(CROSS)/bin/clang
CXX := $(CROSS)/bin/clang++
LD := $(CROSS)/bin/lld-link
LIB := $(CROSS)/bin/llvm-lib
AS := nasm
ANALYZER := scan-build --use-cc=$(CC) --use-c++=$(CXX)

# csv, errorfile, fullhtml, html, tasklist, xml
PVS_FORMAT=fullhtml

# Setup project paths
mkfile_path := $(abspath $(lastword $(MAKEFILE_LIST)))
config_path := $(abspath $(dir $(mkfile_path)))
workspace_path := $(abspath $(config_path)/../)
arch_path := $(abspath $(workspace_path)/kernel/arch/$(VALI_ARCH))

# MollenOS Configuration, comment in or out for specific features
config_flags = 

#########################
# ACPI Configuration
#########################
#config_flags += -DACPI_DEBUG_OUTPUT
#config_flags += -D__OSCONFIG_ACPIDEBUG
#config_flags += -D__OSCONFIG_ACPIDEBUGMUTEXES
#config_flags += -D__OSCONFIG_REDUCEDHARDWARE

#########################
# OS Configuration
#########################
config_flags += -D__OSCONFIG_DEBUGCONSOLE # Enable debug console on startup instead of splash

 # Enable debug mode, this enables the debug mode across kernel, includes extra output and allocates the uart port for debugging.
config_flags += -D__OSCONFIG_DEBUGMODE

config_flags += -D__OSCONFIG_LOGGING_KTRACE # Kernel Tracing
#config_flags += -D__OSCONFIG_DISABLE_SIGNALLING # Kernel fault on all hardware signals
config_flags += -D__OSCONFIG_ENABLE_MULTIPROCESSORS # Use all cores
#config_flags += -D__OSCONFIG_NESTED_INTERRUPTS # Enable nested interrupts in kernel

#config_flags += -D__OSCONFIG_ENABLE_DEBUG_SHORTCUTS
#config_flags += -D__OSCONFIG_RUN_CPPTESTS # Enables user-mode testing programs for the c/c++ suite.
#config_flags += -D__OSCONFIG_TEST_KERNEL  # Enable kernel-mode testing suites of the operating system

#########################
# Driver Configuration
#########################
#config_flags += -D__OSCONFIG_NODRIVERS # Don't load drivers, run it without for debug
#config_flags += -D__OSCONFIG_DISABLE_EHCI # Disable usb 2.0 support, run only in usb 1.1
#config_flags += -D__OSCONFIG_EHCI_ALLOW_64BIT # Allow the EHCI driver to utilize 64 bit dma buffers

# Include correct arch file
include $(dir $(mkfile_path))/$(VALI_ARCH)/rules.mk