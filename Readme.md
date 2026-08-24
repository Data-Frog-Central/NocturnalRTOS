# NocturnalRTOS

This is a repository for CFW made for the DataFrog SF2000 (and GB300 along with other similar devices) using HiChip Semiconductor's HC-FreeRTOS SDK Platform.  

The DataFrog SF2000 (along with other similar devices) is a HCSEMI B210 with a st7789v2 clone lcd. It uses gpio for buttons (and basically everything else on the device).  

NocturnalRTOS is a fork of [AmphibiOS](https://git.maschath.de/ignatz/hcrtos) ([Backup by bnister](https://github.com/bnister/sf2000_hcrtos)) (Specifically the RetroArch branch).  
AmphibiOS is also known as *the HCRTOS* but its kind of a misnomer as it itself is a fork of *the HCRTOS* by HiChip modified to support the SF2000.  

### NocturnalRTOS Status:
RetroArch was kinda ditched and instead I am replacing it with Phobos my own Libretro frontend based on sf2000_multicore's core_api. A lot of hardware has been implemented but not all of it (TV out still doesn't work for example).  

**When will it be done?**  
Im working on this rather slowly in my free time and "done" is rather subjctive, as of right now *some* people might prefer to daily drive this but most wouldn't and as such I would consider this as an open alpha as it't not feature complete yet.

### FAQ:
- Why not use the original name?
    - There is already a lot of confusion surrounding the SF2000 project names (for example 4 sf2000_multicore forks all using the name sf2000_multicore) and I want to avoid any confusion. I will try to make it as explicitly clear as possible that "**this is a fork of HCRTOS by HiChip and AmphibiOS**" as this wouldn't exist without them.

### Attribution/Credit:
These are people that contributed to AmphibiOS/HCRTOS/SF2000 Multicore and without their contributions NocturnalRTOS wouldn't be possible. Osaka, Kobil, and AxelGarciaK have all personally given me a ton of advice and help.

- **Ignatz:** ([Ignatz Donations](https://www.paypal.com/paypalme/ignatzDraconis)) Maintainer (and main dev) of the [AmphibiOS](https://git.maschath.de/ignatz/hcrtos) repo.  
- **xTxNinjaZx AKA Kev:** ([xTxNinjaZx Donations](https://www.paypal.com/paypalme/kkestner91)) Researcher/tester of the current firmware on the SF2000 and Custom Firmware Dev support. 
- **Osaka:** A researcher of the original firmware, author of the bootloader fix and the sample custom code (BMP viewer) along with so much other stuff. 
- **Kobil:** They have contributed a lot of code to sf2000_multicore and hcrtos, a lot of their work has been used as the basis of NocturnalRTOS.
- **AxelGarciaK:** A lot of his and his AI assisted research has helped me learn more about the device I wouldn't have had enough time to learn on my own.
- Every other [GB300 Multicore](https://github.com/tzubertowski/gb300_multicore/graphs/contributors?all=1) and [SF2000 Multicore](https://github.com/madcock/sf2000_multicore/graphs/contributors?all=1) Contributor: There has been so many contributors to the projects over the years and both GB300 and SF2000 Multicore are used as the basis for Phobos and NocturnalRTOS.

*(Donating to these people won't directly contribute to NocturnalRTOS but they are good people who have contributed a lot to the SF2000 community)*  

### Support:
If you want to help with NocturnalRTOS you can reach out to me (Trademarked69) on the [Data Frog Central Discord server](https://trademarked69.github.io/sf2000/discord/) (or the Retro Handhelds Discord server). You can also just leave an issue on the [Issues page]() and/or submit a pull request.

### Information:
**Original SDK Dump and firmware files:**
https://cloud.maschath.de/s/PKfPaHS4qsqewEk  
**General information about the device:**  
https://vonmillhausen.github.io/sf2000/  

### Other Projects:  

**StockFW Patches:**  

- [SF2000 Multicore by Kobil](https://gitlab.com/kobily/sf2000_multicore)  
    - The original SF2000 Multicore project that every other Multicore fork is based on.
- [SF2000 Multicore Forked by Madcock](https://github.com/madcock/sf2000_multicore_cores/releases)
    - Madcock's fork of SF2000 Multicore
- [SF2000 Multicore Forked by Leonardo](https://github.com/leonardothehuman/sf2000_multicore/releases)
    - Leonardo's fork of Madcock's SF2000 Multicore fork
- [SF2000 Multicore Forked by Trademarked69](https://github.com/Trademarked69/sf2000_multicore)
    - Trademarked69's fork of Leonardo's SF2000 Multicore fork
    - Releases found [here](https://github.com/tzubertowski/gb300_multicore/releases) and [here](https://github.com/tzubertowski/FrogUI/releases) (SF2000 Files)
- [GB300 Multicore](https://github.com/tzubertowski/gb300_multicore)
    - fork of Madcock's SF2000 Multicore fork
    - Releases found [here](https://github.com/tzubertowski/gb300_multicore/releases) and [here](https://github.com/tzubertowski/FrogUI/releases) (GB300 Files)  

**Custom Firmware:**  

- [Unifrog by AxelGarciaK](https://github.com/axgdev/UniFrog/releases)
    - Another HCRTOS fork for SF2000
- [frog2k-linux by AxelGarciaK](https://github.com/axgdev/frog2k-linux)
    - Linux on the SF2000
