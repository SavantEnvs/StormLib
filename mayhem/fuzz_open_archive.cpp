// Opens an arbitrary MPQ archive through the public API and walks it the way an embedder would:
// header/table queries, file enumeration, per-file reads (decompression, decryption, sector CRCs),
// per-file and whole-archive verification, and (optionally) a writable open with modify + compact.
// Byte 0 of the input is a configuration byte (stream provider and open flags); the rest is the archive,
// or the container the chosen stream provider unwraps (PART bitmap, BLK4 blocks, MPQE/W3XE encryption).
#include <cstdint>
#include <string>
#include <vector>
#include "StormLib.h"
#include "harness_util.h"

// FileStream_Prefix honours a stream prefix (part-, blk4-, mpqe-, w3xe-, flat-) only when a base provider
// (file:, map:, http:) follows it; a bare "part-" is no prefix at all and names a file that does not exist.
static const char * const kProvider[8] = {"",           "map:",       "part-file:", "blk4-file:",
                                          "mpqe-file:", "w3xe-file:", "flat-file:", "part-map:"};
static const int kProviderBlock4 = 3;
static const int kMaxFiles = 64;
static const DWORD kMaxReadPerFile = 1 << 20;

static void QueryArchive(HANDLE hMpq)
{
    union {
        ULONGLONG u64;
        DWORD u32;
        TMPQHeader hdr;
        BYTE raw[512];
    } v;
    static const SFileInfoClass kClasses[] = {
        SFileMpqHeader,        SFileMpqUserDataHeader, SFileMpqHeaderOffset, SFileMpqHashTableSize,
        SFileMpqBlockTableSize, SFileMpqMaxFileCount,  SFileMpqFileTableSize, SFileMpqSectorSize,
        SFileMpqNumberOfFiles, SFileMpqStreamFlags,    SFileMpqFlags,         SFileMpqSignatures,
        SFileMpqArchiveSize64, SFileMpqHetTableOffset, SFileMpqBetTableOffset, SFileMpqHashTableOffset,
        SFileMpqBlockTableOffset, SFileMpqHiBlockTableOffset, SFileMpqStrongSignatureOffset,
        SFileMpqHetHeader,     SFileMpqBetHeader,
    };
    for(SFileInfoClass c : kClasses)
    {
        DWORD needed = 0;
        SFileGetFileInfo(hMpq, c, &v, sizeof(v), &needed);
    }

    // Tables returned by pointer must be released with SFileFreeFileInfo.
    for(SFileInfoClass c : {SFileMpqHetTable, SFileMpqBetTable})
    {
        void * table = NULL;
        if(SFileGetFileInfo(hMpq, c, &table, sizeof(table), NULL) && table != NULL)
            SFileFreeFileInfo(table, c);
    }
    SFileHasFile(hMpq, "(listfile)");
    SFileHasFile(hMpq, "(attributes)");
}

static void ReadFileFully(HANDLE hMpq, const char * name)
{
    HANDLE hFile = NULL;
    if(!SFileOpenFileEx(hMpq, name, SFILE_OPEN_FROM_MPQ, &hFile))
        return;

    DWORD hi = 0;
    SFileGetFileSize(hFile, &hi);
    char plainName[MAX_PATH];
    SFileGetFileName(hFile, plainName);
    DWORD needed = 0;
    BYTE info[512];
    for(SFileInfoClass c : {SFileInfoFileEntry, SFileInfoPatchChain, SFileInfoLocale, SFileInfoFileSize,
                            SFileInfoCompressedSize, SFileInfoFlags, SFileInfoByteOffset, SFileInfoFileTime})
        SFileGetFileInfo(hFile, c, info, sizeof(info), &needed);

    std::vector<BYTE> buf(0x4000);
    DWORD total = 0;
    while(total < kMaxReadPerFile)
    {
        DWORD got = 0;
        if(!SFileReadFile(hFile, buf.data(), (DWORD)buf.size(), &got, NULL) || got == 0)
            break;
        total += got;
    }

    // Seek around and re-read to exercise sector caching and random access.
    LONG zero = 0;
    SFileSetFilePointer(hFile, 0, &zero, FILE_BEGIN);
    DWORD got = 0;
    SFileReadFile(hFile, buf.data(), 0x100, &got, NULL);
    LONG mid = 0;
    SFileSetFilePointer(hFile, 0x1234, &mid, FILE_BEGIN);
    SFileReadFile(hFile, buf.data(), 0x100, &got, NULL);
    SFileCloseFile(hFile);
}

static void Modify(HANDLE hMpq)
{
    static const char payload[] = "mayhem payload mayhem payload mayhem payload";
    HANDLE hFile = NULL;
    if(SFileCreateFile(hMpq, "mayhem\\added.txt", 0, sizeof(payload), 0, MPQ_FILE_COMPRESS | MPQ_FILE_REPLACEEXISTING, &hFile))
    {
        SFileWriteFile(hFile, payload, sizeof(payload), MPQ_COMPRESSION_ZLIB);
        SFileFinishFile(hFile);
    }
    SFileRenameFile(hMpq, "mayhem\\added.txt", "mayhem\\renamed.txt");
    SFileRemoveFile(hMpq, "(attributes)", 0);
    SFileFlushArchive(hMpq);
    SFileCompactArchive(hMpq, NULL, false);
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t * data, size_t size)
{
    if(size < 1 + 32)
        return -1;
    const uint8_t cfg = data[0];
    const bool writable = (cfg & 0x08) != 0;

    // One directory per input: SFileCompactArchive leaves "<archive>.tmp" behind when it fails, and the
    // stream providers may write sidecar files; removing the directory keeps every input independent.
    std::string dir = make_scratch_dir("open");
    if(dir.empty())
        return -1;
    // Block4Stream_Open strips a trailing ".0" from the name and opens "<name>.0", "<name>.1", ...
    const int provider = cfg & 7;
    std::string path = dir + (provider == kProviderBlock4 ? "/archive.mpq.0" : "/archive.mpq");
    if(!write_file(path, data + 1, size - 1))
    {
        remove_tree(dir);
        return -1;
    }
    std::string name = std::string(kProvider[provider]) + path;

    DWORD flags = MPQ_OPEN_CHECK_SECTOR_CRC | (writable ? 0 : MPQ_OPEN_READ_ONLY);
    if(cfg & 0x10) flags |= MPQ_OPEN_NO_LISTFILE;
    if(cfg & 0x20) flags |= MPQ_OPEN_NO_ATTRIBUTES;
    if(cfg & 0x40) flags |= MPQ_OPEN_FORCE_MPQ_V1;
    if(cfg & 0x80) flags |= MPQ_OPEN_NO_HEADER_SEARCH;

    HANDLE hMpq = NULL;
    int rc = -1;
    if(SFileOpenArchive(name.c_str(), 0, flags, &hMpq))
    {
        rc = 0;
        QueryArchive(hMpq);

        std::vector<std::string> names;
        SFILE_FIND_DATA fd;
        HANDLE hFind = SFileFindFirstFile(hMpq, "*", &fd, NULL);
        if(hFind != NULL)
        {
            do
                names.push_back(fd.cFileName);
            while((int)names.size() < kMaxFiles && SFileFindNextFile(hFind, &fd));
            SFileFindClose(hFind);
        }
        for(const std::string & n : names)
        {
            ReadFileFully(hMpq, n.c_str());
            SFileVerifyFile(hMpq, n.c_str(), SFILE_VERIFY_ALL);
        }
        SFileVerifyArchive(hMpq);
        for(DWORD what = SFILE_VERIFY_MPQ_HEADER; what <= SFILE_VERIFY_HIBLOCK_TABLE; what++)
            SFileVerifyRawData(hMpq, what, NULL);

        if(writable)
            Modify(hMpq);
        SFileCloseArchive(hMpq);
    }
    remove_tree(dir);
    return rc;
}
