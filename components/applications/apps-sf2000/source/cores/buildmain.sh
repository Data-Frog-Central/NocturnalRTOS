### Clean before building
make clean CONSOLE="QuickNES"     CORE=libretro-cores/working-cores/QuickNES_Core # Nintendo Entertainment System / Famicom
make clean CONSOLE="FCEUmm"       CORE=libretro-cores/libretro-fceumm # Famicom Disk System / Nintendo Entertainment System (Fast)
make clean CONSOLE="Snes9x 2002"  CORE=libretro-cores/working-cores/snes9x2002 # Super Nintendo Entertainment System - Super Famicom
make clean CONSOLE="Snes9x 2005"  CORE=libretro-cores/working-cores/snes9x2005 # Super Nintendo Entertainment System (Fast) - Super Famicom (Fast)
make clean CONSOLE="Gambatte"     CORE=libretro-cores/working-cores/libretro-gambatte # Game Boy + Color
make clean CONSOLE="TGB Dual"     CORE=libretro-cores/working-cores/libretro-tgbdual # Game Boy + Color (2P)
make clean CONSOLE="gpSP"         CORE=libretro-cores/working-cores/gpsp # Game Boy Advance
make clean CONSOLE="gpSP-F"       CORE=libretro-cores/working-cores/gpsp-ff # Game Boy Advance (Fast)
make clean CONSOLE="gpSP-FF"      CORE=libretro-cores/working-cores/gpsp-ff EXTRA_CFLAGS="-DSF2000_OPTIMIZATION_LEVEL=3" # Game Boy Advance (FFast)
make clean CONSOLE="PicoDrive"    CORE=libretro-cores/picodrive MAKEFILE=-fMakefile.libretro # Mega Drive - Genesis / Mega-CD - Sega CD
make clean CONSOLE="Gearsystem"   CORE=libretro-cores/Gearsystem/platforms/libretro # Game Gear / Master System - Mark III
make clean CONSOLE="M2k"          CORE=libretro-cores/libretro-mame2000 # Mame 2000
make clean CONSOLE="M2k-N"        CORE=libretro-cores/libretro-mamenummacwaytausend # Mame 2000 (Extra)
make clean CONSOLE="PCE-Fast"     CORE=libretro-cores/libretro-beetle-pce-fast # PC Engine
#make clean CONSOLE="menu"         CORE=libretro-cores/custom-apps/FrogUI # FrogUI

### Build SF2000
make CONSOLE="QuickNES"     CORE=libretro-cores/working-cores/QuickNES_Core # Nintendo Entertainment System / Famicom
make CONSOLE="FCEUmm"       CORE=libretro-cores/libretro-fceumm # Famicom Disk System / Nintendo Entertainment System (Fast)
make CONSOLE="Snes9x 2002"  CORE=libretro-cores/working-cores/snes9x2002 # Super Nintendo Entertainment System - Super Famicom
make CONSOLE="Snes9x 2005"  CORE=libretro-cores/working-cores/snes9x2005 # Super Nintendo Entertainment System (Fast) - Super Famicom (Fast)
make CONSOLE="Gambatte"     CORE=libretro-cores/working-cores/libretro-gambatte # Game Boy + Color
make CONSOLE="TGB Dual"     CORE=libretro-cores/working-cores/libretro-tgbdual # Game Boy + Color (2P)
make CONSOLE="gpSP"         CORE=libretro-cores/working-cores/gpsp # Game Boy Advance
make CONSOLE="gpSP-F"       CORE=libretro-cores/working-cores/gpsp-ff # Game Boy Advance (Fast)
make clean CONSOLE="gpSP-F" CORE=libretro-cores/working-cores/gpsp-ff # Game Boy Advance (Fast) Clean
make CONSOLE="gpSP-FF"      CORE=libretro-cores/working-cores/gpsp-ff EXTRA_CFLAGS="-DSF2000_OPTIMIZATION_LEVEL=3" # Game Boy Advance (FFast)
make CONSOLE="PicoDrive"    CORE=libretro-cores/picodrive MAKEFILE=-fMakefile.libretro # Mega Drive - Genesis / Mega-CD - Sega CD
make CONSOLE="Gearsystem"   CORE=libretro-cores/Gearsystem/platforms/libretro # Game Gear / Master System - Mark III
make CONSOLE="M2k"          CORE=libretro-cores/libretro-mame2000 # Mame 2000
make CONSOLE="M2k-N"        CORE=libretro-cores/libretro-mamenummacwaytausend # Mame 2000 (Extra)
make CONSOLE="PCE-Fast"     CORE=libretro-cores/libretro-beetle-pce-fast # PC Engine
#make CONSOLE="menu"         CORE=libretro-cores/custom-apps/FrogUI # FrogUI

true
