#!/usr/bin/env bash
# mayhem/build.sh — builds StormLib twice (sanitized+SanCov for the fuzz targets, normal flags for the
# oracle), the three libFuzzer harnesses (each also as a -standalone reproducer), and the KAT probe.
set -euo pipefail

[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${SRC:=/mayhem}"
: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${COVERAGE_FLAGS=}"
: "${STANDALONE_FUZZ_MAIN:=/opt/mayhem/StandaloneFuzzTargetMain.c}"
# Two UBSan checks are relaxed; ASan and the rest of UBSan stay halting.
# - alignment: StormLib reads packed MPQ structures (ULONGLONG file times, BSDIFF headers) through casted,
#   unaligned pointers on every valid archive and patch; it would halt on the shipped seeds.
# - function: src/FileStream.cpp stores every stream-provider callback (BlockStream_Read(TBlockStream *, ...),
#   PartStream_Close, W3xeStream_Read, ...) cast to a pointer type taking the base TFileStream *; the first
#   read through a part-/blk4-/mpqe-/w3xe- stream would halt before any provider parsing is reached.
UB_RELAX="-fno-sanitize=alignment,function"
export CC CXX

cd "$SRC"

# 1) Sanitized library. -fsanitize=fuzzer-no-link puts SanCov in the fuzzed code itself; without it the
#    targets build fine and record 0 edges in Mayhem. Bundled libs keep the build offline.
FUZZ_CFLAGS="-O1 $SANITIZER_FLAGS $UB_RELAX $DEBUG_FLAGS -fsanitize=fuzzer-no-link"
cmake -S . -B build/fuzz \
      -DCMAKE_BUILD_TYPE=None \
      -DSTORM_USE_BUNDLED_LIBRARIES=ON -DSTORM_SKIP_INSTALL=ON -DBUILD_SHARED_LIBS=OFF \
      -DCMAKE_C_FLAGS="$FUZZ_CFLAGS" -DCMAKE_CXX_FLAGS="$FUZZ_CFLAGS"
cmake --build build/fuzz -j"$MAYHEM_JOBS"
FUZZ_LIBS=$(find build/fuzz -name '*.a' | sort)
echo "fuzz libs: $FUZZ_LIBS"

# 2) Harnesses: once with the fuzzing engine, once with the run-once driver. The driver is compiled as C so
#    its LLVMFuzzerTestOneInput reference keeps C linkage.
mkdir -p build/fuzz/obj
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o build/fuzz/obj/standalone_main.o
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c mayhem/lsan_off.cc -o build/fuzz/obj/lsan_off.o
for t in fuzz_open_archive fuzz_decompress fuzz_patch; do
  $CXX -O1 $SANITIZER_FLAGS $UB_RELAX $DEBUG_FLAGS -fsanitize=fuzzer-no-link -Isrc -c "mayhem/$t.cpp" -o "build/fuzz/obj/$t.o"
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE "build/fuzz/obj/$t.o" build/fuzz/obj/lsan_off.o \
       -Wl,--start-group $FUZZ_LIBS -Wl,--end-group -o "/mayhem/$t"
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS "build/fuzz/obj/$t.o" build/fuzz/obj/standalone_main.o build/fuzz/obj/lsan_off.o \
       -Wl,--start-group $FUZZ_LIBS -Wl,--end-group -o "/mayhem/$t-standalone"
done

# 3) Oracle: a clean normal-flags library and the KAT probe (plus the seed generator, so it stays buildable).
cmake -S . -B build/test \
      -DCMAKE_BUILD_TYPE=Release \
      -DSTORM_USE_BUNDLED_LIBRARIES=ON -DSTORM_SKIP_INSTALL=ON -DBUILD_SHARED_LIBS=OFF \
      -DCMAKE_C_FLAGS="$COVERAGE_FLAGS" -DCMAKE_CXX_FLAGS="$COVERAGE_FLAGS"
cmake --build build/test -j"$MAYHEM_JOBS"
TEST_LIBS=$(find build/test -name '*.a' | sort)
for t in kat gen_seeds; do
  $CXX -O2 $COVERAGE_FLAGS -Isrc "mayhem/$t.cpp" -Wl,--start-group $TEST_LIBS -Wl,--end-group $COVERAGE_FLAGS \
       -o "/mayhem/$t"
done
