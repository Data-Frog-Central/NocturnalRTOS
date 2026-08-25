all: phobos-all

phobos-all:
	$(MAKE) -f Makefile.common phobos-all_defconfig
	$(MAKE) -f Makefile.common all
	cp output/images/phobos.asd phobos-all.asd

phobos-rotate:
	$(MAKE) -f Makefile.common phobos-rotate_defconfig
	$(MAKE) -f Makefile.common all
	cp output/images/phobos.asd phobos-rotate.asd

clean:
	rm -rf bl output dl
	rm -f *.asd
	rm -f board/hc15xx/common/crc
	rm -f build/tools/genpersistentmem/genpersistentmem
	$(MAKE) -f Makefile.common clean
