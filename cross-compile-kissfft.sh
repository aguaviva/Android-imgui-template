export NDK=~/Android/Sdk/ndk/26.1.10909125
export TOOLCHAIN=$NDK/toolchains/llvm/prebuilt/linux-x86_64
export TARGET=aarch64-linux-android34

export HOST=armelf-unknown-linux-android
export AR=$TOOLCHAIN/bin/llvm-ar
export CC=$TOOLCHAIN/bin/$TARGET$API-clang
export AS=$CC
export CXX=$TOOLCHAIN/bin/$TARGET$API-clang++
export LD=$TOOLCHAIN/bin/ld
export RANLIB=$TOOLCHAIN/bin/llvm-ranlib
export STRIP=$TOOLCHAIN/bin/llvm-strip
export LINK=$TOOLCHAIN/bin/llvm-link
export OBJDUMP=$TOOLCHAIN/bin/llvm-objdump


workdir=$(pwd)
echo *************** $workdir
pushd /tmp

module=kissfft

if [ ! -d $module ]; then
git clone git@github.com:mborgerding/$module.git --depth=1
fi

cd $module
make clean
make PREFIX=$workdir/app/src/main/jni/kissfft/arm64-v8a/  KISSFFT_DATATYPE=float KISSFFT_STATIC=1 KISSFFT_TOOLS=0 KISSFFT_OPENMP=0 CFLAGS=-DNDEBUG=1 install
popd


