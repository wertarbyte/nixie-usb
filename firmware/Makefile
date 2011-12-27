DEVICE  = attiny2313
F_CPU   = 16000000
AVRDUDE = avrdude -p $(DEVICE)
TARGET = nixie-usb

CFLAGS  = -Iusbdrv -I. -DDEBUG_LEVEL=0 -DSUPPORT_ANIMATION
OBJECTS = usbdrv/usbdrv.o usbdrv/usbdrvasm.o usbdrv/oddebug.o $(TARGET).o

COMPILE = avr-gcc -Wall -Os -DF_CPU=$(F_CPU) $(CFLAGS) -mmcu=$(DEVICE)
COMPILEPP = avr-g++ -Wall -Os -DF_CPU=$(F_CPU) $(CFLAGS) -mmcu=$(DEVICE)

# symbolic targets:
help:
	@echo "This Makefile has no default rule. Use one of the following:"
	@echo "make hex ....... to build hex file"
	@echo "make program ... to flash fuses and firmware"
	@echo "make flash ..... to flash the firmware (use this on metaboard)"
	@echo "make clean ..... to delete objects and hex file"

hex: $(TARGET).hex

program: flash

# rule for uploading firmware:
flash: $(TARGET).hex
	$(AVRDUDE) -U flash:w:$<:i

# rule for deleting dependent files (those which can be built by Make):
clean:
	rm -f $(TARGET).hex $(TARGET).lst $(TARGET).obj $(TARGET).cof $(TARGET).list $(TARGET).map $(TARGET).eep.hex $(TARGET).elf *.o usbdrv/*.o $(TARGET).s usbdrv/oddebug.s usbdrv/usbdrv.s

.cpp.o:
	$(COMPILEPP) -c $< -o $@


# Generic rule for compiling C files:
.c.o:
	$(COMPILE) -c $< -o $@

# Generic rule for assembling Assembler source files:
.S.o:
	$(COMPILE) -x assembler-with-cpp -c $< -o $@
# "-x assembler-with-cpp" should not be necessary since this is the default
# file type for the .S (with capital S) extension. However, upper case
# characters are not always preserved on Windows. To ensure WinAVR
# compatibility define the file type manually.

# Generic rule for compiling C to assembler, used for debugging only.
.c.s:
	$(COMPILE) -S $< -o $@

# file targets:

# Since we don't want to ship the driver multipe times, we copy it into this project:
usbdrv:
	cp -r ../../../usbdrv .

$(TARGET).elf: usbdrv $(OBJECTS)	# usbdrv dependency only needed because we copy it
	$(COMPILE) -o $(TARGET).elf $(OBJECTS)

$(TARGET).hex: $(TARGET).elf
	rm -f $(TARGET).hex $(TARGET).eep.hex
	avr-objcopy -j .text -j .data -O ihex $(TARGET).elf $(TARGET).hex
	avr-size $(TARGET).hex

