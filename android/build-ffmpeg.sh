#!/bin/bash
# Build a small static FFmpeg (only what the engine's movie player needs) for
# each Android ABI into app/jni/deps/ffmpeg-build/<abi>/{include,lib}.
#   - 꽃과뱀 opening (hebi_op.vsd): MPEG-1 program stream, mpeg1video + mp2
#   - 동급생 movies (AVI in STREAM.DAT): a few common old AVI codecs
# Needs the Android NDK (ANDROID_NDK_HOME or ANDROID_NDK_LATEST_HOME).
set -euo pipefail
cd "$(dirname "$0")/app/jni/deps"

NDK=${ANDROID_NDK_HOME:-${ANDROID_NDK_LATEST_HOME:-}}
[ -n "$NDK" ] || { echo "ANDROID_NDK_HOME not set" >&2; exit 1; }
TC=$NDK/toolchains/llvm/prebuilt/linux-x86_64
API=21
ABIS=${ABIS:-"arm64-v8a armeabi-v7a x86_64"}

[ -d FFmpeg ] || git clone --depth 1 -b n6.1.2 https://github.com/FFmpeg/FFmpeg.git FFmpeg
OUT=$PWD/ffmpeg-build

for abi in $ABIS; do
	[ -f "$OUT/$abi/lib/libavformat.a" ] && continue
	case $abi in
	arm64-v8a)   arch=aarch64; target=aarch64-linux-android;     extra="" ;;
	armeabi-v7a) arch=arm;     target=armv7a-linux-androideabi;  extra="--cpu=armv7-a --enable-neon" ;;
	x86_64)      arch=x86_64;  target=x86_64-linux-android;      extra="--disable-asm" ;;
	*) echo "unknown ABI $abi" >&2; exit 1 ;;
	esac
	rm -rf "build-$abi" && mkdir "build-$abi" && cd "build-$abi"
	../FFmpeg/configure \
		--prefix="$OUT/$abi" \
		--target-os=android --arch=$arch --enable-cross-compile $extra \
		--cc="$TC/bin/${target}${API}-clang" \
		--cxx="$TC/bin/${target}${API}-clang++" \
		--ar="$TC/bin/llvm-ar" --ranlib="$TC/bin/llvm-ranlib" \
		--nm="$TC/bin/llvm-nm" --strip="$TC/bin/llvm-strip" \
		--enable-pic --enable-static --disable-shared \
		--disable-programs --disable-doc --disable-debug \
		--disable-everything --disable-network --disable-autodetect \
		--disable-avdevice --disable-avfilter --disable-swresample --disable-postproc \
		--enable-avcodec --enable-avformat --enable-avutil --enable-swscale \
		--enable-protocol=file \
		--enable-demuxer=mpegps,mpegvideo,avi,wav,mp3 \
		--enable-parser=mpegvideo,mpegaudio \
		--enable-decoder=mpeg1video,mpeg2video,mp2,mp2float,mp3,mp3float \
		--enable-decoder=pcm_s16le,pcm_u8,adpcm_ms,adpcm_ima_wav \
		--enable-decoder=cinepak,msvideo1,mpeg4,msmpeg4v1,msmpeg4v2,msmpeg4v3,h263,mjpeg,indeo3,indeo4,indeo5
	make -j"$(nproc)"
	make install
	cd ..
done
