all: sf2000

sf2000:
	$(MAKE) -f Makefile.common O=bl sf2000_min_bl_defconfig
	$(MAKE) -f Makefile.common O=bl all
	$(MAKE) -f Makefile.common sf2000_min_defconfig
	$(MAKE) -f Makefile.common all

clean:
	rm -rf bl
	rm -rf output/*
	$(MAKE) -f Makefile.common clean