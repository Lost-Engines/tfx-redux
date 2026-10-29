# Attribution

The RA version 2 decompressor is a C++ adaptation of our GPL-3.0-or-later [did-unpack](https://github.com/Lost-Engines/did-unpack) implementation, which follows Benjamin Haisch’s RA decompression class version 1.0 (2006). His original permission requires credit:

> (c) 2006 Benjamin Haisch. You may use this code freely as long as you give me credit for it :)

The archive lookup/hash and game formats were reconstructed from TFX. Game binaries, archives and extracted resources are not included. The selected gallery in `captures/showcase-4k/` contains screenshots rendered from the original TFX artwork. That artwork remains the property of its respective rights holders and is not covered by this repository’s source-code licence.

Vectorstorm is the bundled Armchair Software shared mathematics component, included without project-specific modifications. Canonical source: https://github.com/Armchair-Software/vectorstorm.

The pinned DOSBox DBOPL and GF1 hardware cores are copyright the DOSBox Team and licensed
under GPL-2.0-or-later. Its source and licence headers are retained in the build;
the small standalone adapter follows our Darker implementation. Munt's MT-32
emulator is linked under its LGPL-2.1-or-later licence, with upstream sources and
licence files retained by CMake's dependency fetch.

The pinned Nuked Sound Canvas emulator is copyright nukeykt and contributors,
including J.C. Moyer's backend integration, under GPL-2.0-or-later. Its original
source headers and licence accompany the fetched dependency. Synthesiser ROMs
are external user assets and are not distributed with this project.

The pinned miniaudio library supplies the shared audio device and streamed WAV,
FLAC and MP3 decoding. Its bundled stb_vorbis supplies Ogg Vorbis decoding.
Both offer a choice of public-domain or MIT licensing; their upstream copyright
and licence headers are retained in the fetched sources.

Creative ASP/CSP QSound reconstruction is source-included from the pinned
[Lost Engines QSound C++](https://github.com/Lost-Engines/qsound-cpp) component,
copyright 2026 Lost Engines contributors. Its source and acknowledgements
remain in the fetched dependency. Original Creative firmware programs are
external research inputs and are not distributed in source or binary packages.
