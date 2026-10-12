all: phobos-all clean phobos-rotate

phobos-all:
	rm -f phobos-all.asd
	$(MAKE) -f Makefile.common phobos-all_defconfig
	$(MAKE) -f Makefile.common all
	cp output/images/phobos.asd phobos-all.asd

phobos-rotate:
	rm -f phobos-rotate.asd
	$(MAKE) -f Makefile.common phobos-rotate_defconfig
	$(MAKE) -f Makefile.common all
	cp output/images/phobos.asd phobos-rotate.asd

clean:
	rm -rf bl output/* dl
	rm -f board/hc15xx/common/crc
	rm -f build/tools/genpersistentmem/genpersistentmem
	$(MAKE) -f Makefile.common clean
