.PHONY: all, monitor, compile, upload, disasm

all: compile upload monitor

compile:
	C:\Users\YuryBatenko\.platformio\penv\Scripts\platformio.exe run --environment genericCH32V003F4P6
	
upload:	
	C:\Users\YuryBatenko\.platformio\penv\Scripts\platformio.exe run --target upload --environment genericCH32V003F4P6

monitor:
	minichlink.exe -T

unbrick :
	minichlink.exe -u

disasm:
	C:\Users\YuryBatenko\.platformio\packages\toolchain-riscv\bin\riscv-none-embed-objdump.exe -S -d .pio/build/genericCH32V003F4P6/firmware.elf > .pio/build/firmware.S