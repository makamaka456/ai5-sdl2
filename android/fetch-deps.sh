#!/bin/bash
# Download the native libraries the Android build needs into app/jni/deps.
set -euo pipefail
cd "$(dirname "$0")/app/jni"
mkdir -p deps && cd deps
clone() { # dir url tag
	[ -d "$1" ] || git clone --depth 1 --recurse-submodules --shallow-submodules -b "$3" "$2" "$1"
}
clone SDL      https://github.com/libsdl-org/SDL.git       release-2.30.9
clone SDL_ttf  https://github.com/libsdl-org/SDL_ttf.git   release-2.22.0
clone SDL_mixer https://github.com/libsdl-org/SDL_mixer.git release-2.8.0
clone libpng   https://github.com/pnggroup/libpng.git      v1.6.44
