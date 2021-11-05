.PHONY: all
all:
	$(MAKE) -C cc OUT=eh3
	$(MAKE) -C ld
	$(MAKE) -C libc OUT=eh3
	$(MAKE) -C emu
	$(MAKE) -C emu/test

	$(MAKE) -C crackmes

	$(MAKE) -C cc.eh3 OUT=eh3
	$(MAKE) -C ld.eh3
	$(MAKE) -C libc.eh3 OUT=eh3
	$(MAKE) -C emu.eh3

.PHONY: clean
clean:
	$(MAKE) clean -C cc
	$(MAKE) clean -C ld
	$(MAKE) clean -C libc
	$(MAKE) clean -C emu
	$(MAKE) clean -C emu/test

	$(MAKE) clean -C crackmes

	$(MAKE) clean -C cc.eh3
	$(MAKE) clean -C ld.eh3
	$(MAKE) clean -C libc.eh3
	$(MAKE) clean -C emu.eh3
