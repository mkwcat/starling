# Clear the implicit built in rules
.SUFFIXES:

# If DEVKITPPC or DEVKITARM is not set, try and set them using DEVKITPRO
ifeq ($(strip $(DEVKITPPC)),)
ifneq ($(strip $(DEVKITPPC)),)
DEVKITPPC := $(DEVKITPRO)/devkitPPC
else
$(error "Please set DEVKITPPC in your environment. export DEVKITPPC=<path to>devkitPPC")
endif
endif

ifeq ($(strip $(DEVKITARM)),)
ifneq ($(strip $(DEVKITPRO)),)
DEVKITARM := $(DEVKITPRO)/devkitARM
else
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif
endif

# Add .d to Make's recognized suffixes.
SUFFIXES += .d

# Project directory
BUILD            := build
LOADER_SOURCES   := loader
LOADER_INCLUDES  := -Iloader -Icommon
CHANNEL_SOURCES  := channel
CHANNEL_INCLUDES := -Ichannel -Iloader -Icommon
IOS_SOURCES      := ios
IOS_INCLUDES     := -Iios -Icommon
COMMON_SOURCES   := common
ASSETS           := $(wildcard assets/*)

# Target module names
TARGET_IOS_MODULE  := $(BUILD)/ios_module
TARGET_IOS_LOADER  := $(BUILD)/ios_loader
TARGET_PPC         := $(BUILD)/starling
TARGET_PPC_STUB    := $(BUILD)/boot

# Data archives
DATA_LOADER        := $(BUILD)/data/loader.arc.lzma
DATA_CHANNEL       := $(BUILD)/data/channel.arc

# Create build directories
DUMMY         := $(shell mkdir -p $(BUILD) \
         $(BUILD)/$(LOADER_SOURCES) \
         $(BUILD)/$(CHANNEL_SOURCES) \
		 $(BUILD)/$(IOS_SOURCES) \
		 $(BUILD)/$(COMMON_SOURCES) \
		 $(DATA_LOADER).d \
		 $(DATA_CHANNEL).d \
		 $(foreach dir, $(ASSETS), $(DATA_CHANNEL).d/$(dir)))

# Compiler definitions
PPC_PREFIX       := $(DEVKITPPC)/bin/powerpc-eabi-
PPC_CC           := $(PPC_PREFIX)gcc
PPC_LD           := $(PPC_PREFIX)ld
PPC_OBJCOPY      := $(PPC_PREFIX)objcopy

LOADER_CFILES    := $(wildcard $(LOADER_SOURCES)/*.c)
LOADER_CPPFILES  := $(wildcard $(LOADER_SOURCES)/*.cpp)
LOADER_SFILES    := $(wildcard $(LOADER_SOURCES)/*.s)
LOADER_OFILES    := $(LOADER_CPPFILES:.cpp=.cpp.ppc.o) $(LOADER_CFILES:.c=.c.ppc.o) \
		    $(LOADER_SFILES:.s=.s.ppc.o)
LOADER_OFILES    := $(addprefix $(BUILD)/, $(LOADER_OFILES))

CHANNEL_CFILES   := $(wildcard $(CHANNEL_SOURCES)/*.c) $(wildcard $(COMMON_SOURCES)/*.c)
CHANNEL_CPPFILES := $(wildcard $(CHANNEL_SOURCES)/*.cpp) $(wildcard $(COMMON_SOURCES)/*.cpp)
CHANNEL_SFILES   := $(wildcard $(CHANNEL_SOURCES)/*.s) $(wildcard $(COMMON_SOURCES)/*.s)
CHANNEL_OFILES   := $(CHANNEL_CPPFILES:.cpp=.cpp.ppc.o) $(CHANNEL_CFILES:.c=.c.ppc.o) \
		    $(CHANNEL_SFILES:.s=.s.ppc.o)
CHANNEL_OFILES   := $(addprefix $(BUILD)/, $(CHANNEL_OFILES))

PPC_LD           := ppc.ld

IOS_PREFIX       := $(DEVKITARM)/bin/arm-none-eabi-
IOS_CC           := $(IOS_PREFIX)gcc
IOS_LD           := $(IOS_PREFIX)ld
IOS_OBJCOPY      := $(IOS_PREFIX)objcopy

IOS_CFILES       := $(wildcard $(IOS_SOURCES)/*.c) $(wildcard $(COMMON_SOURCES)/*.c)
IOS_CPPFILES     := $(wildcard $(IOS_SOURCES)/*.cpp) $(wildcard $(COMMON_SOURCES)/*.cpp)
IOS_SFILES       := $(wildcard $(IOS_SOURCES)/*.s) $(wildcard $(COMMON_SOURCES)/*.s)
IOS_OFILES       := $(IOS_CPPFILES:.cpp=.cpp.ios.o) $(IOS_CFILES:.c=.c.ios.o) \
		    $(IOS_SFILES:.s=.s.ios.o)
IOS_OFILES       := $(addprefix $(BUILD)/, $(IOS_OFILES))

IOS_MODULE_LD    := ios_module.ld
IOS_LOADER_LD    := ios_loader.ld

ASSETFILES       := $(foreach dir, $(ASSETS), $(wildcard $(dir)/*))

WUJ5             := tools/wuj5/wuj5.py
ELF2DOL          := tools/elf2dol.py
INCBIN           := tools/incbin.S

DEPS             := $(LOADER_OFILES:.o=.d) $(CHANNEL_OFILES:.o=.d) $(IOS_OFILES:.o=.d)

# Compiler flags
WFLAGS   := -Wall -Wextra -Wno-unused-const-variable \
	    -Wno-unused-function -Wno-pointer-arith -Wno-narrowing \
	    -Wno-format-truncation -Werror

CFLAGS   := $(INCLUDE) -O1 -fomit-frame-pointer \
            -fno-exceptions -fverbose-asm -ffunction-sections -fdata-sections \
            -fno-builtin-memcpy -fno-builtin-memset \
            $(WFLAGS)

CXXFLAGS := $(CFLAGS) -std=c++20 -fno-rtti

AFLAGS   := -x assembler-with-cpp

IOS_ARCH := -march=armv5te -mtune=arm9tdmi -mthumb-interwork -mbig-endian -mthumb

IOS_LDFLAGS := $(IOS_ARCH) -flto -nostdlib -lgcc -n -Wl,--gc-sections -Wl,-static

PPC_LDFLAGS := -flto -nodefaultlibs -nostdlib -lgcc -n -Wl,--gc-sections -Wl,-static

IOS_DEFS := $(IOS_ARCH) $(IOS_INCLUDES) -DTARGET_IOS
PPC_DEFS := -DTARGET_PPC -Wno-attribute-alias -Wno-missing-attributes

default: $(TARGET_PPC).dol

clean:
	@echo Cleaning: $(BUILD)
	@rm -rf $(BUILD)

-include $(DEPS)

$(BUILD)/$(LOADER_SOURCES)/%.c.ppc.o: $(LOADER_SOURCES)/%.c
	@echo PPC: $<
	@$(PPC_CC) -MMD -MF $(BUILD)/$(LOADER_SOURCES)/$*.c.ppc.d $(CFLAGS) $(PPC_DEFS) $(LOADER_INCLUDES) -c -o $@ $<

$(BUILD)/$(LOADER_SOURCES)/%.cpp.ppc.o: $(LOADER_SOURCES)/%.cpp
	@echo PPC: $<
	@$(PPC_CC) -MMD -MF $(BUILD)/$(LOADER_SOURCES)/$*.cpp.ppc.d $(CXXFLAGS) $(PPC_DEFS) $(LOADER_INCLUDES) -c -o $@ $<

$(BUILD)/$(LOADER_SOURCES)/%.s.ppc.o: $(LOADER_SOURCES)/%.s
	@echo PPC: $<
	@$(PPC_CC) -MMD -MF $(BUILD)/$(LOADER_SOURCES)/$*.s.ppc.d $(AFLAGS) $(PPC_DEFS) $(LOADER_INCLUDES) -c -o $@ $<

$(BUILD)/$(CHANNEL_SOURCES)/%.c.ppc.o: $(CHANNEL_SOURCES)/%.c
	@echo PPC: $<
	@$(PPC_CC) -MMD -MF $(BUILD)/$(CHANNEL_SOURCES)/$*.c.ppc.d $(CFLAGS) $(PPC_DEFS) $(CHANNEL_INCLUDES) -c -o $@ $<

$(BUILD)/$(CHANNEL_SOURCES)/%.cpp.ppc.o: $(CHANNEL_SOURCES)/%.cpp
	@echo PPC: $<
	@$(PPC_CC) -MMD -MF $(BUILD)/$(CHANNEL_SOURCES)/$*.cpp.ppc.d $(CXXFLAGS) $(PPC_DEFS) $(CHANNEL_INCLUDES) -c -o $@ $<

$(BUILD)/$(CHANNEL_SOURCES)/%.s.ppc.o: $(CHANNEL_SOURCES)/%.s
	@echo PPC: $<
	@$(PPC_CC) -MMD -MF $(BUILD)/$(CHANNEL_SOURCES)/$*.s.ppc.d $(AFLAGS) $(PPC_DEFS) $(CHANNEL_INCLUDES) -c -o $@ $<

$(BUILD)/$(COMMON_SOURCES)/%.c.ppc.o: $(COMMON_SOURCES)/%.c
	@echo PPC: $<
	@$(PPC_CC) -MMD -MF $(BUILD)/$(COMMON_SOURCES)/$*.c.ppc.d $(CFLAGS) $(PPC_DEFS) $(CHANNEL_INCLUDES) -c -o $@ $<

$(BUILD)/$(COMMON_SOURCES)/%.cpp.ppc.o: $(COMMON_SOURCES)/%.cpp
	@echo PPC: $<
	@$(PPC_CC) -MMD -MF $(BUILD)/$(COMMON_SOURCES)/$*.cpp.ppc.d $(CXXFLAGS) $(PPC_DEFS) $(CHANNEL_INCLUDES) -c -o $@ $<

$(BUILD)/$(COMMON_SOURCES)/%.s.ppc.o: $(COMMON_SOURCES)/%.s
	@echo PPC: $<
	@$(PPC_CC) -MMD -MF $(BUILD)/$(COMMON_SOURCES)/$*.s.ppc.d $(AFLAGS) $(PPC_DEFS) $(CHANNEL_INCLUDES) -c -o $@ $<

$(BUILD)/%.c.ios.o: %.c
	@echo IOS: $<
	@$(IOS_CC) -MMD -MF $(BUILD)/$*.c.ios.d $(CFLAGS) $(IOS_DEFS) -c -o $@ $<

$(BUILD)/%.cpp.ios.o: %.cpp
	@echo IOS: $<
	@$(IOS_CC) -MMD -MF $(BUILD)/$*.cpp.ios.d $(CXXFLAGS) $(IOS_DEFS) -c -o $@ $<

$(BUILD)/%.s.ios.o: %.s
	@echo IOS: $<
	@$(IOS_CC) -MMD -MF $(BUILD)/$*.s.ios.d $(AFLAGS) $(IOS_DEFS) -c -o $@ $<

# PPC Loader / Channel

$(TARGET_PPC).dol: $(TARGET_PPC).elf
	@echo Output: $(notdir $@)
	@python $(ELF2DOL) $< $@

$(TARGET_PPC).elf: $(LOADER_OFILES) $(CHANNEL_OFILES) $(PPC_LD) $(DATA_LOADER).o $(DATA_CHANNEL).o
	@echo Linking: $(notdir $@)
	@$(PPC_CC) -g -o $@ $(LOADER_OFILES) $(CHANNEL_OFILES) $(DATA_LOADER).o $(DATA_CHANNEL).o -T$(PPC_LD) $(PPC_LDFLAGS) -Wl,-Map,$(TARGET_PPC).map

# IOS Module

$(TARGET_IOS_MODULE)_link.elf: $(IOS_OFILES) $(IOS_MODULE_LD)
	@echo Linking: $(notdir $@)
	@$(IOS_CC) -g $(IOS_OFILES) $(IOS_LDFLAGS) -T$(IOS_MODULE_LD) -o $@

$(TARGET_IOS_MODULE).elf: $(TARGET_IOS_MODULE)_link.elf
	@echo Output: $(notdir $@)
	@$(IOS_CC) -s $(IOS_OFILES) $(IOS_LDFLAGS) -T$(IOS_MODULE_LD) -Wl,-Map,$(TARGET_IOS_MODULE).map -o $@

# IOS Loader

$(TARGET_IOS_LOADER).elf: $(IOS_OFILES) $(IOS_LOADER_LD)
	@echo Linking: $(notdir $@)
	@$(IOS_CC) -g -o $@ $(IOS_OFILES) -T$(IOS_LOADER_LD) $(IOS_LDFLAGS)

$(TARGET_IOS_LOADER).bin: $(TARGET_IOS_LOADER).elf
	@echo Output: $(notdir $@)
	@$(IOS_OBJCOPY) $< $@ -O binary

# Data Archive

$(DATA_LOADER).d/ios_module.elf: $(TARGET_IOS_MODULE).elf
	@cp -f $< $@

$(DATA_LOADER).d/ios_loader.bin: $(TARGET_IOS_LOADER).bin
	@cp -f $< $@

$(DATA_LOADER): $(DATA_LOADER).d/ios_module.elf \
                $(DATA_LOADER).d/ios_loader.bin
	@rm -rf $@
	@echo Packing: $(notdir $@)
	@python $(WUJ5) encode $(DATA_LOADER).d

$(DATA_LOADER).o: $(DATA_LOADER) $(INCBIN)
	@echo PPC: $(notdir $(DATA_LOADER).o)
	@$(PPC_CC) $(INCBIN) -c -o $@ -DPATH=$(DATA_LOADER) -DNAME=LoaderArchive

$(DATA_CHANNEL): $(ASSETFILES)
	@rm -rf $@
	@rm -rf $(DATA_CHANNEL).d/*
	@echo Packing: $(notdir $@)
	@cp -r $(foreach dir, $(ASSETS), $(dir)) $(DATA_CHANNEL).d/
	@python $(WUJ5) encode $(DATA_CHANNEL).d

$(DATA_CHANNEL).o: $(DATA_CHANNEL) $(INCBIN)
	@echo PPC: $(notdir $(DATA_CHANNEL).o)
	@$(PPC_CC) $(INCBIN) -c -o $@ -DPATH=$(DATA_CHANNEL) -DNAME=ResourceArchive -DALIGN=0x20

