Audacious Console Video Game Music Plugin
-----------------------------------------
This plugin plays music from video game consoles of the 1980s and early
1990s. It uses the Game_Music_Emu sound engine and supports the
following file formats:

AY        ZX Spectrum/Amstrad CPC
GBS       Nintendo Game Boy
GYM       Sega Genesis/Mega Drive
HES       NEC TurboGrafx-16/PC Engine
KSS       MSX Home Computer/other Z80 systems (doesn't support FM sound)
NSF/NSFE  Nintendo NES/Famicom (with VRC 6, Namco 106, and FME-7 sound)
SAP       Atari systems using POKEY sound chip
SPC       Super Nintendo/Super Famicom
VGM/VGZ   Sega Master System/Mark III, Sega Genesis/Mega Drive,BBC Micro

Text information tags and track length information is supported in all
formats, though some files might not make use of it.

Most formats include the actual music player code, "ripped" by hand from
the original game to allow it to run on its own. This plugin then
emulates the original processor and sound chip(s) in order to reproduce
the original sound, glitches and all. Band-limited synthesis is used to
give crystal-clear sound quality.

Contact: Shay Green <gblargg@gmail.com>


Notes
-----
* Errors and warnings are logged to stdout, which might help if you want
to figure out why a music file isn't playing properly.

* The HES and KSS music formats lack proper track numbering, presenting
the game's music sometimes randomly scattered among the 256 possible
track numbers. Because of this, most HES and KSS music files are
distributed with an .m3u file in a non-standard format which specifies
which tracks actually have music, among other things. When a game music
file is opened, a corresponding m3u file is opened if in the same
directory. There's not much that we can do about the situation,
unfortunately.


Things not supported
--------------------
* KSS: FM sound

* HES: ADPCM samples (used in only a few soundtracks, if that)

* SAP: "digimusic samples" and more obscure tracker formats

* VGM/VGZ: original Sega Master System FM sound chip (Sega Genesis/Mega
Drive music plays fine). See the following discussion for a way to add
support for this: http://www.smspower.org/forums/viewtopic.php?t=9321


Source code notes
-----------------
Game_Music_Emu library configuration is in blargg_config.h. See
gme_readme.txt and gme.txt for library documentation.

* See TODO comments in Audacious_Config.cxx and Audacious_Driver.cxx for
things which would be good to address.

* C++ exceptions and RTTI are not used or needed.

* File types are determined based on the first four bytes of a file
except for .gym files, some of which have no file header at all.

* Some music formats have more than one track in a file. This track is
specified to the console plugin by appending ?i to the end of the file
path, where i is the track index, starting at 0. For example, foo.nsf?2
specifies the third track. This is an internal thing, as the user should
never see these modified paths.
