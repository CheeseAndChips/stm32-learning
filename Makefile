BUILD_DIR=./Debug
STFLASH=st-flash
TARGET=onewire
OBJCOPY=arm-none-eabi-objcopy
COMPILEDB=compiledb
SED=sed
ELFTARGET=$(BUILD_DIR)/$(TARGET).elf
BINTARGET=$(BUILD_DIR)/$(TARGET).bin

all:
	$(MAKE) -C $(BUILD_DIR) all
	$(OBJCOPY) -O binary $(ELFTARGET) $(BINTARGET)

.PHONY: flash
flash: all
	$(STFLASH) write $(BINTARGET) 0x8000000
	$(STFLASH) reset

.PHONY: compiledb
compiledb: clean
	$(COMPILEDB) -o ./compile_commands.json make -j8 all
	$(SED) 's/-fcyclomatic-complexity/-I\/usr\/include/g' -i ./compile_commands.json

.PHONY: clean
clean:
	$(MAKE) -C $(BUILD_DIR) clean
	$(RM) $(BINTARGET)
