#!/bin/bash
# Fetch the reference CD images and external audio firmware; verify every asset on each run.
set -u
usage() { echo "Usage: $0 [destination]"; echo 'Fetch the TFX 2.05 CD, QSound firmware and CM-32L, SC-55 and SCC-1A ROMs (default: $XDG_DATA_HOME/tfx or ~/.local/share/tfx).'; }
if [[ "${1:-}" == --help ]]; then usage; exit 0; fi
if (( $# > 1 )); then usage >&2; exit 2; fi
script_dir=$(cd -- "$(dirname -- "$0")" && pwd) || exit 1
data_home=${XDG_DATA_HOME:-$HOME/.local/share}
# Match the runtime and Darker: XDG_DATA_HOME must be an absolute path.
if [[ "$data_home" != /* ]]; then data_home="$HOME/.local/share"; fi
destination=${1:-$data_home/tfx}
for manifest in game-files.txt roms.txt; do
  if [[ ! -r "$script_dir/$manifest" ]]; then echo "ERROR: missing asset manifest: $script_dir/$manifest" >&2; exit 1; fi
done
for dependency in curl unzip; do
  if ! command -v "$dependency" >/dev/null; then echo "ERROR: install $dependency to fetch assets." >&2; exit 1; fi
done
if command -v sha256sum >/dev/null; then hash_command=(sha256sum);
elif command -v shasum >/dev/null; then hash_command=(shasum -a 256);
else echo 'ERROR: install sha256sum or shasum to verify assets.' >&2; exit 1; fi
mkdir -p -- "$destination" || exit 1
cd -- "$destination" || exit 1
temporary=$(mktemp -d .fetch-assets.XXXXXXXX) || exit 1
trap 'rm -rf -- "$temporary"' EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

matches() {
  [[ -f "$1" ]] || return 1
  local digest
  digest=$("${hash_command[@]}" -- "$1") || return 1
  [[ "${digest%% *}" == "$expected" ]]
}
download() {
  curl --user-agent 'Lost Engines TFX Redux' --fail --location \
    --retry 3 --retry-all-errors --retry-delay 2 --connect-timeout 30 \
    --output "$2" "$1"
}
archive_url=''
archive_ready=0
fetch() {
  if matches "$file"; then echo "$file: OK"; return 0; fi
  if [[ -d "$file" ]]; then echo "ERROR: $file is a directory." >&2; return 1; fi
  echo "Fetching $file"
  mkdir -p -- "$(dirname -- "$file")" || return 1
  if [[ "$url" == - ]]; then
    cp -- "$script_dir/$member" "$temporary/rom.tmp" || return 1
  elif [[ "$member" == - ]]; then
    download "$url" "$temporary/rom.tmp" || return 1
  else
    # Cache this archive once for its selected members, including a failed
    # attempt. Extract just the named ROM, never the unrelated archive contents.
    if [[ "$archive_url" != "$url" ]]; then
      archive_url=$url
      archive_ready=0
      if download "$url" "$temporary/archive.zip"; then archive_ready=1; fi
    fi
    (( archive_ready )) || return 1
    unzip -p "$temporary/archive.zip" "$member" > "$temporary/rom.tmp" || return 1
  fi
  if ! matches "$temporary/rom.tmp"; then echo "ERROR: $file failed SHA-256 verification." >&2; return 1; fi
  mv -f -- "$temporary/rom.tmp" "$file"
}
failed=0
for manifest in game-files.txt roms.txt; do
while read -r expected file url member; do
  [[ -z "$expected" || "$expected" == \#* ]] && continue
  member=${member%$'\r'}
  if ! fetch; then echo "ERROR: could not fetch a verified $file. Rerun this script to retry." >&2; failed=1; fi
done < "$script_dir/$manifest"
done
if (( failed )); then exit 1; fi
echo "Game images and audio firmware ready in $PWD"
echo "Launch TFX normally; it will prepare the game files from cd/tfx.cue on first use."
