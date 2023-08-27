BUILD_DIR=./Debug
STFLASH=st-flash
TARGET=onewire
OBJCOPY=arm-none-eabi-objcopy
COMPILEDB=compiledb
SED=sed
ELFTARGET=$(BUILD_DIR)/$(TARGET).elf
BINTARGET=$(BUILD_DIR)/$(TARGET).bin
JOBCNT=-j4

all:
	$(MAKE) -C $(BUILD_DIR) $(JOBCNT) all
	$(OBJCOPY) -O binary $(ELFTARGET) $(BINTARGET)

.PHONY: flash
flash: all
	$(STFLASH) write $(BINTARGET) 0x8000000
	$(STFLASH) reset

.PHONY: compiledb
compiledb: clean
	$(COMPILEDB) -o ./compile_commands.json make $(JOBCNT) all
	$(SED) 's/-fcyclomatic-complexity/-I\/usr\/include/g' -i ./compile_commands.json

.PHONY: clean
clean:
	$(MAKE) -C $(BUILD_DIR) clean
	$(RM) $(BINTARGET)
	$(RM) $(BUILD_DIR)/font.hex
	$(RM) $(BUILD_DIR)/font_special.hex
	$(RM) $(BUILD_DIR)/font_data.o
	$(RM) $(BUILD_DIR)/font_special_data.o
