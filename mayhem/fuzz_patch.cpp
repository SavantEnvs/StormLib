// Applies an arbitrary incremental patch to a fixed base archive. The input is the raw patch file
// ("PTCH" header, MD5 block, XFRM block with a COPY or BSD0 body). Each input is stored into a fresh patch
// MPQ through the public archive-writing API, the patch MPQ is attached to the base with
// SFileOpenPatchArchive, and the patched file is read back, which runs the patch loader and applier.
#include <cstdint>
#include <string>
#include <vector>
#include "StormLib.h"
#include "harness_util.h"

static const char kBaseName[] = "data\\base.bin";
static std::string g_basePath;

static void MakeBaseArchive()
{
    g_basePath = reserve_scratch("base");
    unlink(g_basePath.c_str());
    HANDLE hMpq = NULL;
    if(!SFileCreateArchive(g_basePath.c_str(), MPQ_CREATE_ARCHIVE_V1, 16, &hMpq))
        abort();
    std::vector<uint8_t> content(0x2000);
    for(size_t i = 0; i < content.size(); i++)
        content[i] = (uint8_t)((i * 7 + (i >> 8)) & 0xFF);
    HANDLE hFile = NULL;
    if(!SFileCreateFile(hMpq, kBaseName, 0, (DWORD)content.size(), 0, MPQ_FILE_COMPRESS | MPQ_FILE_REPLACEEXISTING, &hFile))
        abort();
    if(!SFileWriteFile(hFile, content.data(), (DWORD)content.size(), MPQ_COMPRESSION_ZLIB) || !SFileFinishFile(hFile))
        abort();
    SFileCloseArchive(hMpq);
}

static void RemoveBase() { unlink(g_basePath.c_str()); }

extern "C" int LLVMFuzzerInitialize(int *, char ***)
{
    MakeBaseArchive();
    atexit(RemoveBase);
    return 0;
}

static bool StorePatchBlob(const std::string & patchPath, const uint8_t * data, size_t size)
{
    HANDLE hMpq = NULL;
    if(!SFileCreateArchive(patchPath.c_str(), MPQ_CREATE_ARCHIVE_V1, 16, &hMpq))
        return false;
    HANDLE hFile = NULL;
    if(SFileCreateFile(hMpq, kBaseName, 0, (DWORD)size, 0, MPQ_FILE_PATCH_FILE | MPQ_FILE_REPLACEEXISTING, &hFile))
    {
        SFileWriteFile(hFile, data, (DWORD)size, 0);
        SFileFinishFile(hFile);
    }
    SFileCloseArchive(hMpq);
    return true;
}

static void ReadPatched(HANDLE hBase)
{
    HANDLE hf = NULL;
    if(!SFileOpenFileEx(hBase, kBaseName, SFILE_OPEN_FROM_MPQ, &hf))
        return;
    std::vector<uint8_t> buf(0x10000);
    DWORD got = 0;
    DWORD total = 0;
    while(total < (1u << 24) && SFileReadFile(hf, buf.data(), (DWORD)buf.size(), &got, NULL) && got != 0)
        total += got;
    SFileCloseFile(hf);
}

// Input is either a complete patch MPQ (starts with "MPQ\x1a"), used as is, or a bare patch file that is
// wrapped into a fresh patch MPQ. The writer only flags BSD0 patches as patch files; the MPQ form is how
// COPY patches and hand-built TPatchInfo headers get here.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t * data, size_t size)
{
    const bool isMpq = size >= 4 && !memcmp(data, "MPQ\x1a", 4);
    if(size < (isMpq ? 0x20u : 0x44u) || size > (1 << 20))
        return -1;

    // One directory per input, removed whole at the end, so no sidecar file the library creates survives.
    std::string dir = make_scratch_dir("patch");
    if(dir.empty())
        return -1;
    std::string patchPath = dir + "/patch.mpq";
    if(isMpq ? !write_file(patchPath, data, size) : !StorePatchBlob(patchPath, data, size))
    {
        remove_tree(dir);
        return -1;
    }

    int rc = -1;
    HANDLE hBase = NULL;
    if(SFileOpenArchive(g_basePath.c_str(), 0, MPQ_OPEN_READ_ONLY, &hBase))
    {
        if(SFileOpenPatchArchive(hBase, patchPath.c_str(), NULL, 0))
        {
            rc = 0;
            ReadPatched(hBase);
        }
        SFileCloseArchive(hBase);
    }
    remove_tree(dir);
    return rc;
}
