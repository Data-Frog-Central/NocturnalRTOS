#!/bin/bash
cd libretro-cores
cd working-cores/QuickNES_Core && git submodule update --init . && cd ../../
cd libretro-fceumm && git submodule update --init . && cd ../
cd working-cores/snes9x2002 && git submodule update --init . && cd ../../
cd working-cores/snes9x2005 && git submodule update --init . && cd ../../
cd working-cores/libretro-gambatte && git submodule update --init . && cd ../../
cd working-cores/libretro-tgbdual && git submodule update --init . && cd ../../
cd working-cores/gpsp && git submodule update --init . && cd ../../
cd working-cores/gpsp-ff && git submodule update --init . && cd ../../
cd picodrive && git submodule update --init . && cd ../
cd Gearsystem && git submodule update --init . && cd ../
cd libretro-mame2000 && git submodule update --init . && cd ../
cd libretro-mamenummacwaytausend && git submodule update --init . && cd ../
cd libretro-beetle-pce-fast && git submodule update --init . && cd ../
cd ../
