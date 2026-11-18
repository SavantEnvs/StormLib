#!/usr/bin/env bash
#
# mayhem/test.sh — known-answer oracle for StormLib (library built by mayhem/build.sh with normal flags).
# /mayhem/kat drives the public API on committed fixtures in mayhem/kat/ (generated once by mayhem/gen_seeds.cpp);
# every expected value below is compared HERE, in bash, with the checksums of the original inputs computed
# independently of StormLib (text and pattern generators documented next to each constant).
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
cd "$SRC"

emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

KAT=/mayhem/kat
FX=mayhem/kat
if [ ! -x "$KAT" ]; then
  echo "test.sh: $KAT missing — mayhem/build.sh must build it" >&2
  emit_ctrf "storm-kat" 0 1 0
  exit 1
fi

passed=0
failed=0
check() {  # check <name> <expected> <actual>
  if [ "$2" == "$3" ]; then
    passed=$((passed + 1))
  else
    failed=$((failed + 1))
    printf 'FAIL %s\n  expected: %s\n  actual:   %s\n' "$1" "$2" "$3" >&2
  fi
}
sha() { sha256sum | cut -c1-64; }

# Original inputs (checksums computed with Python, not with StormLib):
#   TEXT    "The quick brown fox jumps over the lazy dog.\n" x 40            (1800 bytes)
#   PAT(n)  byte i = (i*7 + (i>>8)) & 0xFF                                    (n bytes)
#   SPARSE  12288 zero bytes with 0xA0+i at 0x100+i and 0x50+i at 0x1800+i, i<16
TEXT=b7e385dd443946d0b3faf1ae28f6ded004889dd070f92542d60422056962dd0e
PAT300=8ed4c5152b4e33d77773b3b3779f0e2b572e58660c62cf0db4acdfc85f691a9f
PAT800=76de9e1233c1e351dd6ea927f0ae21ec2eea81065e40143b4303a2f44019f6da
PAT900=e003aa1a01e53ac6a101d7028ef83cf85d2b794dba73218bb3b1e24af3bca255
PAT3000=f3760c5bb9b37324b7020799a92bf5e48ddc0cf442070e7b75b2acb075473d6e
PAT6000=ab3cea7760ba32f3fa5e5a0d18914d1c1c4eac1ead7eaa8b7a616e76f70d96dd
SPARSE=67cb73c0dd071bd002abd14f07f1ae689f755463495f54f89f6399da671caab2
# Patched file: PAT(0x2000) with bytes 0x400..0x417 = 0xC0+i and byte 0x1FFF ^= 0x55
PATCHED=b0f95b573f5ae843ceab92784f4777d97cbc9499c64596fa3269cf7e46083d3b
PAT2000=f7d0d9a971f4d6c8771823e043041e3738c735b160934ac54a34b858e8c2a558
# ADPCM is lossy: these two are regression values of the decoder output for the generated waves.
WAVE_MONO=f79deb5283d29a9d5f000ce0abe25dc0af9dcafdc8706bf960207236c46a2e2e
WAVE_STEREO=708b34ed66ed4ab0959fd0bf14499b2c2fa3029505804c76ccaaba18a308b387

# File contents out of archives (every codec/encryption/layout the writer offers)
cat_sha() { "$KAT" cat "$FX/$1.mpq" "$2" | sha; }
check "v1 readme.txt (zlib+crc)"        "$TEXT"    "$(cat_sha v1 readme.txt)"
check "v1 secret.txt (encrypted)"       "$TEXT"    "$(cat_sha v1 secret.txt)"
check "v1 plain.bin (stored)"           "$PAT300"  "$(cat_sha v1 plain.bin)"
check "v1 data\\pattern.bin (implode)"  "$PAT3000" "$(cat_sha v1 'data\pattern.bin')"
check "v1 tone.wav (adpcm mono+huff)"   "$WAVE_MONO" "$(cat_sha v1 'sound\tone.wav')"
check "v2 readme.txt (bzip2)"           "$TEXT"    "$(cat_sha v2 readme.txt)"
check "v2 data\\pattern.bin (zlib)"     "$PAT3000" "$(cat_sha v2 'data\pattern.bin')"
check "v2 stereo.wav (adpcm stereo)"    "$WAVE_STEREO" "$(cat_sha v2 stereo.wav)"
check "v3 readme.txt"                   "$TEXT"    "$(cat_sha v3 readme.txt)"
check "v3 unit.bin (single unit)"       "$PAT900"  "$(cat_sha v3 unit.bin)"
check "v3 sparse.bin (sparse+zlib)"     "$SPARSE"  "$(cat_sha v3 sparse.bin)"
check "v4 readme.txt (lzma)"            "$TEXT"    "$(cat_sha v4 readme.txt)"
check "v4 data\\pattern.bin (bzip2)"    "$PAT3000" "$(cat_sha v4 'data\pattern.bin')"
check "v4 sparse.bin (sparse+bzip2)"    "$SPARSE"  "$(cat_sha v4 sparse.bin)"
check "v4 unit.bin (lzma single unit)"  "$PAT900"  "$(cat_sha v4 unit.bin)"
check "v4 secret.txt (encrypted)"       "$TEXT"    "$(cat_sha v4 secret.txt)"
check "tiny a.txt"                      "$TEXT"    "$(cat_sha tiny a.txt)"
check "readme.txt is the exact text"    "The quick brown fox jumps over the lazy dog." "$("$KAT" cat "$FX/v1.mpq" readme.txt | head -n 1)"
check "readme.txt size"                 1800       "$("$KAT" cat "$FX/v2.mpq" readme.txt | wc -c | tr -d ' ')"

# Archive headers and file tables
check "v1 info" "version=0 sector=4096 files=7"  "$("$KAT" info "$FX/v1.mpq")"
check "v2 info" "version=1 sector=4096 files=5"  "$("$KAT" info "$FX/v2.mpq")"
check "v3 info" "version=2 sector=16384 files=5" "$("$KAT" info "$FX/v3.mpq")"
check "v4 info" "version=3 sector=16384 files=8" "$("$KAT" info "$FX/v4.mpq")"
check "v4 list" "12288 data\\pattern.bin|2304 unit.bin|1800 secret.txt|233 (attributes)|72 (signature)|1800 readme.txt|64 (listfile)|12288 sparse.bin" \
  "$("$KAT" list "$FX/v4.mpq" | paste -sd'|')"
check "v1 list" "2048 sound\\tone.wav|12288 data\\pattern.bin|768 plain.bin|1800 secret.txt|204 (attributes)|1800 readme.txt|69 (listfile)" \
  "$("$KAT" list "$FX/v1.mpq" | paste -sd'|')"

# Signature and per-file CRC/MD5 verification (weak signature verifies, v1 has none)
check "v4 verify" "archive=2|file data\\pattern.bin 336|file unit.bin 336|file secret.txt 336|file (attributes) 256|file (signature) 768|file readme.txt 336|file (listfile) 336|file sparse.bin 336" \
  "$("$KAT" verify "$FX/v4.mpq" | paste -sd'|')"
check "v1 verify" "archive=0|file sound\\tone.wav 240|file data\\pattern.bin 80|file plain.bin 80|file secret.txt 80|file (attributes) 0|file readme.txt 84|file (listfile) 80" \
  "$("$KAT" verify "$FX/v1.mpq" | paste -sd'|')"

# Single sector decompression through SCompDecompress2 (blobs produced by SCompCompress)
decomp_sha() { "$KAT" decomp "$FX/sector_$1.bin" "$2" | sha; }
check "sector zlib"          "$TEXT"        "$(decomp_sha zlib 1800)"
check "sector bzip2"         "$TEXT"        "$(decomp_sha bzip2 1800)"
check "sector lzma"          "$TEXT"        "$(decomp_sha lzma 1800)"
check "sector pkware"        "$PAT800"      "$(decomp_sha pkware 2048)"
check "sector sparse"        "$SPARSE"      "$(decomp_sha sparse 12288)"
check "sector sparse+zlib"   "$SPARSE"      "$(decomp_sha sparse_zlib 12288)"
check "sector sparse+bzip2"  "$SPARSE"      "$(decomp_sha sparse_bzip2 12288)"
check "sector adpcm mono"    "$WAVE_MONO"   "$(decomp_sha huff_adpcm_mono 2048)"
check "sector adpcm stereo"  "$WAVE_STEREO" "$(decomp_sha huff_adpcm_stereo 4096)"
if "$KAT" decomp "$FX/sector_zlib.bin" 10 >/dev/null 2>&1; then
  check "decompress into a too small buffer is refused" refused accepted
else
  check "decompress into a too small buffer is refused" refused refused
fi

# Incremental patch (BSD0 diff against the base file, MD5 of the result verified by the library)
check "base file before patching" "$PAT2000"  "$("$KAT" cat "$FX/patch_base.mpq" 'data\base.bin' | sha)"
check "BSD0 patched file"         "$PATCHED"  "$("$KAT" patch "$FX/patch_base.mpq" "$FX/patch_bsd0.mpq" 'data\base.bin' | sha)"
check "COPY patch keeps the file" "$PAT2000"  "$("$KAT" patch "$FX/patch_base.mpq" "$FX/patch_copy.mpq" 'data\base.bin' | sha)"

# Stream providers: wrap.mpq (readme.txt + a stored PAT(0x6000) that crosses block boundaries) wrapped by
# gen_seeds in a PART bitmap, BLK4 blocks and MPQE (Salsa20) encryption. Each must open through its
# "<provider>-file:" prefix with the original contents; without the prefix the stored file must not come back
# intact (so the provider, not the plain-file path, did the unwrapping). A bare "part-" (no base provider) is
# no prefix to StormLib: it names a missing file.
WRAPLIST="1800 readme.txt|24576 data\\stored.bin|29 (listfile)"
for spec in "$FX/wrap.mpq" "part-file:$FX/wrap.part" "blk4-file:$FX/wrap_blk4.mpq.0" "mpqe-file:$FX/wrap.mpqe" "part-map:$FX/wrap.part" "flat-file:$FX/wrap.mpq"; do
  check "$spec info"       "version=1 sector=4096 files=3" "$("$KAT" info "$spec")"
  check "$spec list"       "$WRAPLIST" "$("$KAT" list "$spec" | paste -sd'|')"
  check "$spec readme.txt" "$TEXT"     "$("$KAT" cat "$spec" readme.txt | sha)"
  check "$spec stored.bin" "$PAT6000"  "$("$KAT" cat "$spec" 'data\stored.bin' | sha)"
done
for spec in "$FX/wrap.part" "$FX/wrap_blk4.mpq.0" "$FX/wrap.mpqe" "part-$FX/wrap.part"; do
  got=$("$KAT" cat "$spec" 'data\stored.bin' 2>/dev/null | sha)
  if [ "$got" == "$PAT6000" ]; then check "$spec needs its provider" garbled intact; else check "$spec needs its provider" garbled garbled; fi
done

# A corrupted archive must be rejected, not half-opened
bad=$(mktemp)
head -c 100 "$FX/v1.mpq" > "$bad"
if "$KAT" info "$bad" >/dev/null 2>&1; then check "truncated archive is rejected" rejected accepted; else check "truncated archive is rejected" rejected rejected; fi
rm -f "$bad"

emit_ctrf "storm-kat" "$passed" "$failed" 0
