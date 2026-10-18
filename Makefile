# NumMasterGear (Master System / Game Gear) pour NumWorks - Makefile inspiré de l'exemple officiel
# https://github.com/numworks/epsilon-sample-app-c
#
#   make clean && make build              ->  output/NumMasterGear.nwa
#   make check                            ->  vérifie que l'app se lie pour la calculatrice
#                                             (échoue si elle demande trop de RAM)
#

Q ?= @

CC = arm-none-eabi-gcc

BUILD_DIR = output

NWLINK = npx --yes -- nwlink@0.0.16

LINK_GC = 1

define object_for
$(addprefix $(BUILD_DIR)/,$(addsuffix .o,$(basename $(1))))
endef

STORAGE ?= 1

src = $(addprefix src/,main.c meta.c stubs.c) $(wildcard src/sms/*.c)
ifeq ($(STORAGE),1)
src += src/rle.c
endif

EADK_CFLAGS := $(shell $(NWLINK) eadk-cflags)

CFLAGS = -std=gnu99
CFLAGS += $(EADK_CFLAGS)
CFLAGS += -O2 -Wall
CFLAGS += -DLSB_FIRST=1 -DPLATFORM_DEVICE=1 -Isrc -Isrc/sms
CFLAGS += -DNW_ENABLE_STORAGE=$(STORAGE) -DNW_BARE=0

LDFLAGS = -Wl,--relocatable
LDFLAGS += -nostartfiles
LDFLAGS += --specs=nano.specs

ifeq ($(LINK_GC),1)
CFLAGS += -fdata-sections -ffunction-sections
LDFLAGS += -Wl,-e,main -Wl,-u,eadk_app_name -Wl,-u,eadk_app_icon -Wl,-u,eadk_api_level
LDFLAGS += -Wl,--gc-sections
endif

.PHONY: build
build: $(BUILD_DIR)/NumMasterGear.nwa

.PHONY: check
check: $(BUILD_DIR)/NumMasterGear.bin

# Fausse ROM de 32 Ko, uniquement pour le test de liaison
$(BUILD_DIR)/dummy.rom: | $(BUILD_DIR)
	$(Q) head -c 32768 /dev/zero > $@

$(BUILD_DIR)/%.bin: $(BUILD_DIR)/%.nwa $(BUILD_DIR)/dummy.rom
	@echo "BIN $@"
	$(Q) $(NWLINK) nwa-bin --external-data $(BUILD_DIR)/dummy.rom $< $@

$(BUILD_DIR)/NumMasterGear.nwa: $(call object_for,$(src)) $(BUILD_DIR)/icon.o
	@echo "LD  $@"
	$(Q) $(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

$(addprefix $(BUILD_DIR)/,%.o): %.c | $(BUILD_DIR)
	@echo "CC  $^"
	$(Q) $(CC) $(CFLAGS) -c $^ -o $@

$(BUILD_DIR)/icon.o: icon.png | $(BUILD_DIR)
	@echo "ICON $<"
	$(Q) $(NWLINK) png-icon-o $< $@

.PRECIOUS: $(BUILD_DIR)

$(BUILD_DIR):
	$(Q) mkdir -p $@/src/sms

.PHONY: clean
clean:
	@echo "CLEAN"
	$(Q) rm -rf $(BUILD_DIR)
