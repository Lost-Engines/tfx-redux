# Attribution

The RA version 2 decompressor is a C++ adaptation of our GPL-3.0-or-later [did-unpack](https://github.com/Lost-Engines/did-unpack) implementation, which follows Benjamin Haisch’s RA decompression class version 1.0 (2006). His original permission requires credit:

> (c) 2006 Benjamin Haisch. You may use this code freely as long as you give me credit for it :)

The archive lookup/hash and game formats were reconstructed from TFX. Game binaries, archives, extracted resources and audiovisual assets are not included.

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
copyright 2026 Lost Engines contributors, under BSD-3-Clause. Its source, notice
and licence remain in the fetched dependency. Original Creative firmware is an
external asset and is not distributed in source or binary packages.

BSD 3-Clause License

Copyright (c) 2026, Lost Engines contributors
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.
3. Neither the name of the copyright holder nor the names of its contributors
   may be used to endorse or promote products derived from this software
   without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
