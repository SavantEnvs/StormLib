// Deterministic generator for the committed seed corpora and the KAT fixtures, built with StormLib's own
// writers. Usage: gen_seeds <mayhem-dir>. The output is committed; build.sh only compiles this tool so
// it cannot rot. Seeds land in <dir>/<target>/testsuite/, fixtures in <dir>/kat/.
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>
#include "StormLib.h"

typedef std::vector<uint8_t> Bytes;

static void Die(const char * what)
{
    fprintf(stderr, "gen_seeds: %s failed\n", what);
    exit(1);
}

static void Put32(Bytes & b, uint32_t v)
{
    for(int i = 0; i < 4; i++)
        b.push_back((uint8_t)(v >> (8 * i)));
}

static void Put64(Bytes & b, uint64_t v)
{
    for(int i = 0; i < 8; i++)
        b.push_back((uint8_t)(v >> (8 * i)));
}

static void Append(Bytes & b, const void * p, size_t n)
{
    b.insert(b.end(), (const uint8_t *)p, (const uint8_t *)p + n);
}

//-----------------------------------------------------------------------------
// Test content (the formulas are repeated in mayhem/test.sh's expected values)

static Bytes Pattern(size_t n)
{
    Bytes b(n);
    for(size_t i = 0; i < n; i++)
        b[i] = (uint8_t)((i * 7 + (i >> 8)) & 0xFF);
    return b;
}

static Bytes Text()
{
    static const char line[] = "The quick brown fox jumps over the lazy dog.\n";
    Bytes b;
    for(int i = 0; i < 40; i++)
        Append(b, line, sizeof(line) - 1);
    return b;
}

static Bytes SparseData()
{
    Bytes b(0x3000, 0);
    for(size_t i = 0; i < 16; i++)
    {
        b[0x100 + i] = (uint8_t)(0xA0 + i);
        b[0x1800 + i] = (uint8_t)(0x50 + i);
    }
    return b;
}

static Bytes Wave(bool stereo)
{
    Bytes b;
    int v = 0, step = 600;
    for(int i = 0; i < 0x400; i++)
    {
        v += step;
        if(v > 12000 || v < -12000)
            step = -step;
        for(int c = 0; c < (stereo ? 2 : 1); c++)
        {
            int s = (c == 0) ? v : -v / 2;
            b.push_back((uint8_t)(s & 0xFF));
            b.push_back((uint8_t)((s >> 8) & 0xFF));
        }
    }
    return b;
}

//-----------------------------------------------------------------------------
// MD5 (RFC 1321), needed for the MD5 block of the patch header

static Bytes Md5(const Bytes & msg)
{
    static const uint32_t K[64] = {
        0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
        0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
        0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
        0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
        0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
        0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
        0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
        0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391};
    static const int R[64] = {7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
                              5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20,
                              4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
                              6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};
    uint32_t a0 = 0x67452301, b0 = 0xefcdab89, c0 = 0x98badcfe, d0 = 0x10325476;
    Bytes m = msg;
    m.push_back(0x80);
    while(m.size() % 64 != 56)
        m.push_back(0);
    Put64(m, (uint64_t)msg.size() * 8);
    for(size_t off = 0; off < m.size(); off += 64)
    {
        uint32_t w[16];
        for(int i = 0; i < 16; i++)
            w[i] = m[off + i * 4] | (m[off + i * 4 + 1] << 8) | (m[off + i * 4 + 2] << 16) | ((uint32_t)m[off + i * 4 + 3] << 24);
        uint32_t a = a0, b = b0, c = c0, d = d0;
        for(int i = 0; i < 64; i++)
        {
            uint32_t f;
            int g;
            if(i < 16)      { f = (b & c) | (~b & d); g = i; }
            else if(i < 32) { f = (d & b) | (~d & c); g = (5 * i + 1) % 16; }
            else if(i < 48) { f = b ^ c ^ d;          g = (3 * i + 5) % 16; }
            else            { f = c ^ (b | ~d);       g = (7 * i) % 16; }
            f = f + a + K[i] + w[g];
            a = d; d = c; c = b;
            b = b + ((f << R[i]) | (f >> (32 - R[i])));
        }
        a0 += a; b0 += b; c0 += c; d0 += d;
    }
    Bytes out;
    Put32(out, a0); Put32(out, b0); Put32(out, c0); Put32(out, d0);
    return out;
}

//-----------------------------------------------------------------------------
// Output helpers

static std::string g_root;

static void MakeDirs(const std::string & path)
{
    for(size_t i = 1; i < path.size(); i++)
        if(path[i] == '/')
            mkdir(path.substr(0, i).c_str(), 0755);
    mkdir(path.c_str(), 0755);
}

static void WriteFileBytes(const std::string & path, const Bytes & b)
{
    FILE * f = fopen(path.c_str(), "wb");
    if(f == NULL || fwrite(b.data(), 1, b.size(), f) != b.size())
        Die(path.c_str());
    fclose(f);
}

static Bytes ReadFileBytes(const std::string & path)
{
    Bytes b;
    FILE * f = fopen(path.c_str(), "rb");
    if(f == NULL)
        Die(path.c_str());
    uint8_t buf[4096];
    size_t n;
    while((n = fread(buf, 1, sizeof(buf), f)) > 0)
        b.insert(b.end(), buf, buf + n);
    fclose(f);
    return b;
}

static std::string TargetDir(const char * target)
{
    std::string d = g_root + "/" + target + "/testsuite";
    MakeDirs(d);
    return d;
}

static std::string KatDir()
{
    std::string d = g_root + "/kat";
    MakeDirs(d);
    return d;
}

//-----------------------------------------------------------------------------
// MPQ archives

struct Entry
{
    const char * name;
    Bytes data;
    DWORD flags;
    DWORD compression;
};

static const ULONGLONG kFileTime = 0x01D5A0B0C0D0E0F0ULL;

static void MakeMpq(const std::string & path, DWORD createFlags, DWORD maxFiles, const std::vector<Entry> & entries, bool sign)
{
    HANDLE hMpq = NULL;
    unlink(path.c_str());
    if(!SFileCreateArchive(path.c_str(), createFlags, maxFiles, &hMpq))
        Die("SFileCreateArchive");
    for(const Entry & e : entries)
    {
        HANDLE hFile = NULL;
        if(!SFileCreateFile(hMpq, e.name, kFileTime, (DWORD)e.data.size(), 0, e.flags | MPQ_FILE_REPLACEEXISTING, &hFile))
            Die(e.name);
        if(!SFileWriteFile(hFile, e.data.data(), (DWORD)e.data.size(), e.compression))
            Die(e.name);
        if(!SFileFinishFile(hFile))
            Die(e.name);
    }
    if(sign && !SFileSignArchive(hMpq, SIGNATURE_TYPE_WEAK))
        Die("SFileSignArchive");
    if(!SFileCloseArchive(hMpq))
        Die("SFileCloseArchive");
}

static Bytes WithConfig(uint8_t cfg, const Bytes & archive)
{
    Bytes b;
    b.push_back(cfg);
    b.insert(b.end(), archive.begin(), archive.end());
    return b;
}

static void ArchiveSet(const std::string & seedDir)
{
    const std::string kat = KatDir();
    const DWORD C = MPQ_FILE_COMPRESS | MPQ_FILE_SECTOR_CRC;
    std::vector<Entry> v1 = {
        {"readme.txt", Text(), C, MPQ_COMPRESSION_ZLIB},
        {"data\\pattern.bin", Pattern(0x3000), MPQ_FILE_IMPLODE, 0},
        {"sound\\tone.wav", Wave(false), MPQ_FILE_COMPRESS, MPQ_COMPRESSION_ADPCM_MONO | MPQ_COMPRESSION_HUFFMANN},
        {"secret.txt", Text(), MPQ_FILE_COMPRESS | MPQ_FILE_ENCRYPTED | MPQ_FILE_KEY_V2, MPQ_COMPRESSION_ZLIB},
        {"plain.bin", Pattern(0x300), 0, 0},
    };
    MakeMpq(kat + "/v1.mpq", MPQ_CREATE_ARCHIVE_V1 | MPQ_CREATE_LISTFILE | MPQ_CREATE_ATTRIBUTES, 32, v1, false);

    std::vector<Entry> v2 = {
        {"readme.txt", Text(), C, MPQ_COMPRESSION_BZIP2},
        {"data\\pattern.bin", Pattern(0x3000), C, MPQ_COMPRESSION_ZLIB},
        {"stereo.wav", Wave(true), MPQ_FILE_COMPRESS, MPQ_COMPRESSION_ADPCM_STEREO | MPQ_COMPRESSION_HUFFMANN},
    };
    MakeMpq(kat + "/v2.mpq", MPQ_CREATE_ARCHIVE_V2 | MPQ_CREATE_LISTFILE | MPQ_CREATE_ATTRIBUTES, 16, v2, false);

    std::vector<Entry> v3 = {
        {"readme.txt", Text(), C, MPQ_COMPRESSION_ZLIB},
        {"sparse.bin", SparseData(), MPQ_FILE_COMPRESS, MPQ_COMPRESSION_SPARSE | MPQ_COMPRESSION_ZLIB},
        {"unit.bin", Pattern(0x900), MPQ_FILE_COMPRESS | MPQ_FILE_SINGLE_UNIT, MPQ_COMPRESSION_ZLIB},
    };
    MakeMpq(kat + "/v3.mpq", MPQ_CREATE_ARCHIVE_V3 | MPQ_CREATE_LISTFILE | MPQ_CREATE_ATTRIBUTES, 16, v3, false);

    std::vector<Entry> v4 = {
        {"readme.txt", Text(), C, MPQ_COMPRESSION_LZMA},
        {"data\\pattern.bin", Pattern(0x3000), MPQ_FILE_COMPRESS | MPQ_FILE_SECTOR_CRC, MPQ_COMPRESSION_BZIP2},
        {"sparse.bin", SparseData(), MPQ_FILE_COMPRESS, MPQ_COMPRESSION_SPARSE | MPQ_COMPRESSION_BZIP2},
        {"unit.bin", Pattern(0x900), MPQ_FILE_COMPRESS | MPQ_FILE_SINGLE_UNIT, MPQ_COMPRESSION_LZMA},
        {"secret.txt", Text(), MPQ_FILE_COMPRESS | MPQ_FILE_ENCRYPTED, MPQ_COMPRESSION_ZLIB},
    };
    MakeMpq(kat + "/v4.mpq", MPQ_CREATE_ARCHIVE_V4 | MPQ_CREATE_LISTFILE | MPQ_CREATE_ATTRIBUTES | MPQ_CREATE_SIGNATURE, 16, v4, true);

    // Small archive for the signature path and a deliberately tiny one for cheap mutations.
    std::vector<Entry> tiny = {{"a.txt", Text(), MPQ_FILE_COMPRESS, MPQ_COMPRESSION_ZLIB}};
    MakeMpq(kat + "/tiny.mpq", MPQ_CREATE_ARCHIVE_V1, 4, tiny, false);

    // Seeds: configuration byte 0 = plain file, read-only; 0x08 = writable (modify + compact path).
    WriteFileBytes(seedDir + "/v1.mpq.cfg0", WithConfig(0x00, ReadFileBytes(kat + "/v1.mpq")));
    WriteFileBytes(seedDir + "/v2.mpq.cfg0", WithConfig(0x00, ReadFileBytes(kat + "/v2.mpq")));
    WriteFileBytes(seedDir + "/v3.mpq.cfg0", WithConfig(0x00, ReadFileBytes(kat + "/v3.mpq")));
    WriteFileBytes(seedDir + "/v4.mpq.cfg0", WithConfig(0x00, ReadFileBytes(kat + "/v4.mpq")));
    WriteFileBytes(seedDir + "/tiny.mpq.cfg0", WithConfig(0x00, ReadFileBytes(kat + "/tiny.mpq")));
    WriteFileBytes(seedDir + "/v1.mpq.cfg8", WithConfig(0x08, ReadFileBytes(kat + "/v1.mpq")));
    WriteFileBytes(seedDir + "/v4.mpq.cfg8", WithConfig(0x08, ReadFileBytes(kat + "/v4.mpq")));
    WriteFileBytes(seedDir + "/v3.mpq.map", WithConfig(0x01, ReadFileBytes(kat + "/v3.mpq")));
}

//-----------------------------------------------------------------------------
// Stream-provider containers (src/FileStream.cpp). Each wraps a finished archive so that the provider named
// by the prefix (part-file:, blk4-file:, mpqe-file:) has to unwrap it before the MPQ parser sees a byte.

// PART: PART_FILE_HEADER (version 2, ASCII build number, file size, power-of-two block size), then one
// PART_FILE_MAP_ENTRY {Flags=3, BlockOffsLo, BlockOffsHi, LargeValueLo, LargeValueHi} per block, then the
// blocks. Every block is padded to the full block size so the last one can be read whole.
static Bytes PartWrap(const Bytes & mpq)
{
    const uint32_t kBlock = 0x1000;
    const uint32_t blocks = (uint32_t)((mpq.size() + kBlock - 1) / kBlock);
    Bytes b;
    Put32(b, 2);
    char build[0x20] = "10958";
    Append(b, build, sizeof(build));
    Put32(b, 0);
    Put32(b, (uint32_t)mpq.size());
    Put32(b, 0);
    Put32(b, kBlock);
    const uint32_t dataStart = (uint32_t)b.size() + blocks * 20;
    for(uint32_t i = 0; i < blocks; i++)
    {
        Put32(b, 3);
        Put32(b, dataStart + i * kBlock);
        Put32(b, 0);
        Put32(b, 0);
        Put32(b, 0);
    }
    for(uint32_t i = 0; i < blocks; i++)
    {
        size_t off = (size_t)i * kBlock;
        size_t n = std::min<size_t>(kBlock, mpq.size() - off);
        b.insert(b.end(), mpq.begin() + off, mpq.begin() + off + n);
        b.resize(b.size() + (kBlock - n), 0);
    }
    return b;
}

// BLK4: the archive cut into 0x4000-byte blocks, each followed by a 0x20-byte hash slot (written as the
// block's MD5 in hex; the reader skips it), all in "<name>.0".
static Bytes Block4Wrap(const Bytes & mpq)
{
    const size_t kBlock = 0x4000;
    Bytes b;
    for(size_t off = 0; off < mpq.size(); off += kBlock)
    {
        size_t n = std::min(kBlock, mpq.size() - off);
        Bytes blk(mpq.begin() + off, mpq.begin() + off + n);
        b.insert(b.end(), blk.begin(), blk.end());
        Bytes md5 = Md5(blk);
        for(uint8_t x : md5)
        {
            static const char hex[] = "0123456789abcdef";
            b.push_back((uint8_t)hex[x >> 4]);
            b.push_back((uint8_t)hex[x & 15]);
        }
    }
    return b;
}

// MPQE: the archive encrypted with the Salsa20 variant of MpqeStream (the first known auth code), which
// MpqeStream_BruteForce finds by decrypting the first block to a v2 MPQ header. The stream cipher only
// covers whole 64-byte blocks, so the archive is zero-padded to a multiple of 64 (the MPQ header bounds it).
static uint32_t Rol(uint32_t v, int n) { return (v << n) | (v >> (32 - n)); }
#define QR(a, b, c, d) b ^= Rol(a + d, 7); c ^= Rol(b + a, 9); d ^= Rol(c + b, 13); a ^= Rol(d + c, 18);

static Bytes MpqeWrap(const Bytes & mpq)
{
    static const char kTemplate[] = "expand 32-byte k000000000000000000000000000000000000000000000000";
    static const char kAuth[] = "S48B6CDTN5XEQAKQDJNDLJBJ73FDFM3U";
    uint32_t key[16], auth[8];
    memcpy(key, kTemplate, sizeof(key));
    memcpy(auth, kAuth, sizeof(auth));
    key[4 + 0x00] = auth[3]; key[4 + 0x02] = auth[7]; key[4 + 0x03] = auth[2]; key[4 + 0x05] = auth[6];
    key[4 + 0x06] = auth[1]; key[4 + 0x08] = auth[5]; key[4 + 0x09] = auth[0]; key[4 + 0x0B] = auth[4];
    key[5] = 0;
    key[8] = 0;

    Bytes b = mpq;
    b.resize((b.size() + 63) & ~(size_t)63, 0);
    for(size_t off = 0; off < b.size(); off += 64)
    {
        const uint32_t * m = key;
        uint32_t k[16];
        k[0x0E] = m[0x00]; k[0x0C] = m[0x01]; k[0x05] = m[0x02]; k[0x0F] = m[0x03];
        k[0x0A] = m[0x04]; k[0x07] = m[0x05]; k[0x0B] = m[0x06]; k[0x09] = m[0x07];
        k[0x03] = m[0x08]; k[0x06] = m[0x09]; k[0x08] = m[0x0A]; k[0x0D] = m[0x0B];
        k[0x02] = m[0x0C]; k[0x04] = m[0x0D]; k[0x01] = m[0x0E]; k[0x00] = m[0x0F];
        for(int i = 0; i < 20; i += 2)
        {
            QR(k[0x0E], k[0x0A], k[0x03], k[0x02]);
            QR(k[0x0C], k[0x07], k[0x06], k[0x04]);
            QR(k[0x05], k[0x0B], k[0x08], k[0x01]);
            QR(k[0x0F], k[0x09], k[0x0D], k[0x00]);
            QR(k[0x0E], k[0x04], k[0x08], k[0x09]);
            QR(k[0x0C], k[0x01], k[0x0D], k[0x0A]);
            QR(k[0x05], k[0x00], k[0x03], k[0x07]);
            QR(k[0x0F], k[0x02], k[0x06], k[0x0B]);
        }
        const uint32_t ks[16] = {
            k[0x0E] + m[0x00], k[0x04] + m[0x0D], k[0x08] + m[0x0A], k[0x09] + m[0x07],
            k[0x0A] + m[0x04], k[0x0C] + m[0x01], k[0x01] + m[0x0E], k[0x0D] + m[0x0B],
            k[0x03] + m[0x08], k[0x07] + m[0x05], k[0x05] + m[0x02], k[0x00] + m[0x0F],
            k[0x02] + m[0x0C], k[0x06] + m[0x09], k[0x0B] + m[0x06], k[0x0F] + m[0x03]};
        for(int i = 0; i < 64; i++)
            b[off + i] ^= (uint8_t)(ks[i / 4] >> (8 * (i % 4)));
        if(++key[8] == 0)
            key[5]++;
    }
    return b;
}
#undef QR

static void ProviderSet(const std::string & seedDir)
{
    const std::string kat = KatDir();
    // A v2 archive (MpqeStream_BruteForce wants a v2 header at offset 0) with a stored file that crosses
    // several PART and BLK4 block boundaries, so only a provider that really unwraps reproduces it.
    std::vector<Entry> w = {
        {"readme.txt", Text(), MPQ_FILE_COMPRESS | MPQ_FILE_SECTOR_CRC, MPQ_COMPRESSION_ZLIB},
        {"data\\stored.bin", Pattern(0x6000), 0, 0},
    };
    MakeMpq(kat + "/wrap.mpq", MPQ_CREATE_ARCHIVE_V2 | MPQ_CREATE_LISTFILE, 8, w, false);
    const Bytes mpq = ReadFileBytes(kat + "/wrap.mpq");
    const Bytes part = PartWrap(mpq), blk4 = Block4Wrap(mpq), mpqe = MpqeWrap(mpq);
    WriteFileBytes(kat + "/wrap.part", part);
    WriteFileBytes(kat + "/wrap_blk4.mpq.0", blk4);
    WriteFileBytes(kat + "/wrap.mpqe", mpqe);

    // Configuration byte bits 0-2 select the provider (fuzz_open_archive.cpp kProvider): 2 part-file:,
    // 3 blk4-file:, 4 mpqe-file:, 6 flat-file:, 7 part-map:.
    WriteFileBytes(seedDir + "/wrap.part.cfg2", WithConfig(0x02, part));
    WriteFileBytes(seedDir + "/wrap.blk4.cfg3", WithConfig(0x03, blk4));
    WriteFileBytes(seedDir + "/wrap.mpqe.cfg4", WithConfig(0x04, mpqe));
    WriteFileBytes(seedDir + "/v1.mpq.cfg6", WithConfig(0x06, ReadFileBytes(kat + "/v1.mpq")));
    WriteFileBytes(seedDir + "/wrap.part.cfg7", WithConfig(0x07, part));
}

//-----------------------------------------------------------------------------
// Raw compressed sectors

static Bytes Compress(const Bytes & in, unsigned mask, int level)
{
    Bytes out(in.size() + 0x400);
    int cbOut = (int)out.size();
    // SCompCompress refuses an output buffer smaller than the input; the result is what it wrote.
    if(!SCompCompress(out.data(), &cbOut, (void *)in.data(), (int)in.size(), mask, 0, level))
        Die("SCompCompress");
    out.resize(cbOut);
    return out;
}

static void DecompressSet(const std::string & seedDir)
{
    const std::string kat = KatDir();
    struct Item { const char * name; Bytes data; unsigned mask; int level; };
    std::vector<Item> items = {
        {"zlib", Text(), MPQ_COMPRESSION_ZLIB, -1},
        {"bzip2", Text(), MPQ_COMPRESSION_BZIP2, -1},
        {"pkware", Pattern(0x800), MPQ_COMPRESSION_PKWARE, -1},
        {"sparse", SparseData(), MPQ_COMPRESSION_SPARSE, -1},
        {"sparse_zlib", SparseData(), MPQ_COMPRESSION_SPARSE | MPQ_COMPRESSION_ZLIB, -1},
        {"sparse_bzip2", SparseData(), MPQ_COMPRESSION_SPARSE | MPQ_COMPRESSION_BZIP2, -1},
        {"lzma", Text(), MPQ_COMPRESSION_LZMA, -1},
        {"huff_adpcm_mono", Wave(false), MPQ_COMPRESSION_ADPCM_MONO | MPQ_COMPRESSION_HUFFMANN, 4},
        {"huff_adpcm_stereo", Wave(true), MPQ_COMPRESSION_ADPCM_STEREO | MPQ_COMPRESSION_HUFFMANN, 4},
    };
    for(const Item & it : items)
    {
        Bytes comp = Compress(it.data, it.mask, it.level);

        // Round trip through the decompressor under test; the lossy ADPCM pair only has to keep its size.
        Bytes back(it.data.size());
        int cbBack = (int)back.size();
        if(!SCompDecompress2(back.data(), &cbBack, comp.data(), (int)comp.size()) || cbBack != (int)it.data.size())
            Die(it.name);
        if(!(it.mask & (MPQ_COMPRESSION_ADPCM_MONO | MPQ_COMPRESSION_ADPCM_STEREO)) && back != it.data)
            Die(it.name);

        Bytes seed;
        seed.push_back((uint8_t)((it.data.size() - 1) & 0xFF));
        seed.push_back((uint8_t)(((it.data.size() - 1) >> 8) & 0xFF));
        seed.insert(seed.end(), comp.begin(), comp.end());
        WriteFileBytes(seedDir + "/" + it.name, seed);
        WriteFileBytes(kat + "/sector_" + it.name + ".bin", comp);
    }
}

//-----------------------------------------------------------------------------
// Patches

static Bytes BsdiffBlob(const Bytes & oldData, const Bytes & newData)
{
    Bytes bs;
    Append(bs, "BSDIFF40", 8);
    Put64(bs, 12);
    Put64(bs, newData.size());
    Put64(bs, newData.size());
    Put32(bs, (uint32_t)newData.size());
    Put32(bs, 0);
    Put32(bs, 0);
    for(size_t i = 0; i < newData.size(); i++)
        bs.push_back((uint8_t)(newData[i] - (i < oldData.size() ? oldData[i] : 0)));
    return bs;
}

// The patch loader's own run-length coding: a zero-run byte (n-1) skips n zeros, a literal byte
// (0x80 | n-1) is followed by n bytes; the stream starts with a DWORD that the decoder skips.
static Bytes RleBlob(const Bytes & raw)
{
    Bytes out;
    Put32(out, (uint32_t)raw.size());
    size_t i = 0;
    while(i < raw.size())
    {
        size_t j = i;
        if(raw[i] == 0)
        {
            while(j < raw.size() && raw[j] == 0 && j - i < 128)
                j++;
            out.push_back((uint8_t)(j - i - 1));
        }
        else
        {
            while(j < raw.size() && raw[j] != 0 && j - i < 128)
                j++;
            out.push_back((uint8_t)(0x80 | (j - i - 1)));
            out.insert(out.end(), raw.begin() + i, raw.begin() + j);
        }
        i = j;
    }
    return out;
}

static Bytes PatchFile(const Bytes & oldData, const Bytes & newData, const Bytes & body, const char * type, size_t sizeOfPatchData)
{
    Bytes p;
    Append(p, "PTCH", 4);
    Put32(p, (uint32_t)sizeOfPatchData);
    Put32(p, (uint32_t)oldData.size());
    Put32(p, (uint32_t)newData.size());
    Append(p, "MD5_", 4);
    Put32(p, 0x28);
    Bytes m1 = Md5(oldData), m2 = Md5(newData);
    p.insert(p.end(), m1.begin(), m1.end());
    p.insert(p.end(), m2.begin(), m2.end());
    Append(p, "XFRM", 4);
    Put32(p, (uint32_t)(12 + body.size()));
    Append(p, type, 4);
    p.insert(p.end(), body.begin(), body.end());
    return p;
}

static const char kPatchName[] = "data\\base.bin";

static void PatchSet(const std::string & seedDir)
{
    const std::string kat = KatDir();
    Bytes oldData = Pattern(0x2000);
    Bytes newData = oldData;
    for(size_t i = 0; i < 24; i++)
        newData[0x400 + i] = (uint8_t)(0xC0 + i);
    newData[0x1FFF] ^= 0x55;

    std::vector<Entry> base = {{kPatchName, oldData, MPQ_FILE_COMPRESS, MPQ_COMPRESSION_ZLIB}};
    MakeMpq(kat + "/patch_base.mpq", MPQ_CREATE_ARCHIVE_V1, 8, base, false);

    Bytes bs = BsdiffBlob(oldData, newData);
    Bytes rle = RleBlob(bs);
    Bytes bsd0 = PatchFile(oldData, newData, rle, "BSD0", 68 + bs.size());
    WriteFileBytes(seedDir + "/bsd0.ptch", bsd0);

    // Patch MPQs. The writer only flags BSD0 files as patches, so the COPY variant is written as BSD0 and its
    // type tag is flipped in the finished (uncompressed) archive.
    std::vector<Entry> pe = {{kPatchName, bsd0, MPQ_FILE_PATCH_FILE | MPQ_FILE_COMPRESS, 0}};
    MakeMpq(kat + "/patch_bsd0.mpq", MPQ_CREATE_ARCHIVE_V1, 8, pe, false);

    Bytes copyBody = rle;  // COPY keeps the previous contents, so the patch declares an unchanged file
    Bytes copyPatch = PatchFile(oldData, oldData, copyBody, "BSD0", 68 + copyBody.size());
    std::vector<Entry> ce = {{kPatchName, copyPatch, MPQ_FILE_PATCH_FILE | MPQ_FILE_COMPRESS, 0}};
    MakeMpq(kat + "/patch_copy.mpq", MPQ_CREATE_ARCHIVE_V1, 8, ce, false);
    Bytes mpq = ReadFileBytes(kat + "/patch_copy.mpq");
    size_t hits = 0;
    for(size_t i = 0; i + 4 <= mpq.size(); i++)
        if(!memcmp(&mpq[i], "BSD0", 4))
        {
            memcpy(&mpq[i], "COPY", 4);
            hits++;
        }
    if(hits != 1)
        Die("COPY tag flip");
    WriteFileBytes(kat + "/patch_copy.mpq", mpq);

    WriteFileBytes(seedDir + "/patch_bsd0.mpq", ReadFileBytes(kat + "/patch_bsd0.mpq"));
    WriteFileBytes(seedDir + "/patch_copy.mpq", mpq);
    WriteFileBytes(kat + "/patch.bin", bsd0);
}

int main(int argc, char ** argv)
{
    if(argc != 2)
    {
        fprintf(stderr, "usage: gen_seeds <mayhem-dir>\n");
        return 2;
    }
    g_root = argv[1];
    ArchiveSet(TargetDir("fuzz_open_archive"));
    ProviderSet(TargetDir("fuzz_open_archive"));
    DecompressSet(TargetDir("fuzz_decompress"));
    PatchSet(TargetDir("fuzz_patch"));
    return 0;
}
