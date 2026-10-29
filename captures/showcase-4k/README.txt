TFX Redux — 4K screenshot gallery
124 original engine renders at 3840 x 2160; RGB lossless PNG.

Open index.html directly in a browser. Search, camera/light/collection filters,
highlights and a browser-local shortlist help select frames. Arrow keys browse
the selected images. Escape closes the full-frame viewer.

images/             Original 4K PNGs for social media and other uses.
thumbs/             Small JPEGs for browsing; not the source images.
contact-sheets/     Numbered overview sheets, up to 24 frames per sheet.
contact-sheet.jpg   One overview of the entire collection.
captions.csv        Captions, subjects, tags, resolution and image checksums.
manifest.json       Exact settings, recipes, source/build hashes and timestamps.
scene-library.json  Composition inventory for repeat captures (in the repository).
recipes/            Per-image staging recipes.

All views use Redux's ordinary rendering pipeline and the original game assets.
Cameras, formations, weapons, effects and poses are staged for composition.
No image upscaling, AI-generated imagery, retouching or compositing was used.
The original 2D panels retain their native pixel detail. Several scenes use
scripted animation states rather than recording unscripted gameplay.

The vortex shots use the MiG-29, MiG-21 and F-16's authored SSD events 15/16,
which are invoked by the original AI manoeuvre scripts. Those parameter meanings
do not transfer to all player aircraft. The scenes demonstrate the visual state,
not an independently measured G threshold.

Keep this directory together when copying to shared storage. It contains no
install media, game archives, ROMs or user saves. The selected images are tracked
in Git LFS and published at https://lost-engines.github.io/tfx-redux/.
Capture logs, scratch output and other capture directories remain local.
Original TFX artwork belongs to its respective rights holders.

To rerender selected scenes with the debug-enabled Redux build:
python3 tools/capture_showcase.py --data-dir /path/to/TFX \
  --manifest /path/to/gallery/scene-library.json --output /path/to/gallery \
  --only SCENE_ID

The builder always uses private temporary preferences. Re-rendering regenerates
the basic catalogue; the supplied packaging script can restore this expanded
catalogue. It requires Python 3 and Pillow, only for rebuilding, not browsing.
