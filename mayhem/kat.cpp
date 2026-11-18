// Known-answer probe: drives the public StormLib API on committed fixtures and prints what it computed.
// mayhem/test.sh compares that output with exact expected values from bash.
//   kat info <mpq>                  format version, sector size, file count
//   kat list <mpq>                  "<size> <name>" per file, in archive order
//   kat cat <mpq> <name>            raw file contents on stdout
//   kat decomp <blob> <outsize>     SCompDecompress2 output on stdout
//   kat patch <base> <patch> <name> contents of <name> in base with the patch MPQ applied
//   kat verify <mpq>                archive verification code and the per-file CRC/MD5 verdicts
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include "StormLib.h"

static int Fail(const char * what)
{
    fprintf(stderr, "kat: %s failed (error %u)\n", what, (unsigned)SErrGetLastError());
    return 1;
}

static bool Slurp(const char * path, std::vector<unsigned char> & out)
{
    FILE * f = fopen(path, "rb");
    if(f == NULL)
        return false;
    unsigned char buf[4096];
    size_t n;
    while((n = fread(buf, 1, sizeof(buf), f)) > 0)
        out.insert(out.end(), buf, buf + n);
    fclose(f);
    return true;
}

static int DumpFile(HANDLE hMpq, const char * name)
{
    HANDLE hFile = NULL;
    if(!SFileOpenFileEx(hMpq, name, SFILE_OPEN_FROM_MPQ, &hFile))
        return Fail("SFileOpenFileEx");
    unsigned char buf[4096];
    DWORD got = 0;
    for(;;)
    {
        // A short final read returns FALSE with ERROR_HANDLE_EOF; the bytes it delivered still count.
        bool ok = SFileReadFile(hFile, buf, sizeof(buf), &got, NULL);
        if(got != 0)
            fwrite(buf, 1, got, stdout);
        if(!ok || got == 0)
            break;
    }
    SFileCloseFile(hFile);
    return 0;
}

int main(int argc, char ** argv)
{
    if(argc < 3)
        return 2;
    const std::string cmd = argv[1];

    if(cmd == "decomp" && argc == 4)
    {
        std::vector<unsigned char> in;
        if(!Slurp(argv[2], in))
            return Fail("read blob");
        int cbOut = atoi(argv[3]);
        std::vector<unsigned char> out(cbOut);
        if(!SCompDecompress2(out.data(), &cbOut, in.data(), (int)in.size()))
            return Fail("SCompDecompress2");
        fwrite(out.data(), 1, cbOut, stdout);
        return 0;
    }

    HANDLE hMpq = NULL;
    if(!SFileOpenArchive(argv[2], 0, MPQ_OPEN_READ_ONLY | MPQ_OPEN_CHECK_SECTOR_CRC, &hMpq))
        return Fail("SFileOpenArchive");
    int rc = 0;

    if(cmd == "info" && argc == 3)
    {
        TMPQHeader hdr;
        DWORD sector = 0, files = 0;
        SFileGetFileInfo(hMpq, SFileMpqHeader, &hdr, sizeof(hdr), NULL);
        SFileGetFileInfo(hMpq, SFileMpqSectorSize, &sector, sizeof(sector), NULL);
        SFileGetFileInfo(hMpq, SFileMpqNumberOfFiles, &files, sizeof(files), NULL);
        printf("version=%u sector=%u files=%u\n", (unsigned)hdr.wFormatVersion, (unsigned)sector, (unsigned)files);
    }
    else if(cmd == "list" && argc == 3)
    {
        SFILE_FIND_DATA fd;
        HANDLE hFind = SFileFindFirstFile(hMpq, "*", &fd, NULL);
        if(hFind != NULL)
        {
            do
                printf("%u %s\n", (unsigned)fd.dwFileSize, fd.cFileName);
            while(SFileFindNextFile(hFind, &fd));
            SFileFindClose(hFind);
        }
    }
    else if(cmd == "cat" && argc == 4)
    {
        rc = DumpFile(hMpq, argv[3]);
    }
    else if(cmd == "patch" && argc == 5)
    {
        if(!SFileOpenPatchArchive(hMpq, argv[3], NULL, 0))
            rc = Fail("SFileOpenPatchArchive");
        else
            rc = DumpFile(hMpq, argv[4]);
    }
    else if(cmd == "verify" && argc == 3)
    {
        printf("archive=%u\n", (unsigned)SFileVerifyArchive(hMpq));
        SFILE_FIND_DATA fd;
        HANDLE hFind = SFileFindFirstFile(hMpq, "*", &fd, NULL);
        if(hFind != NULL)
        {
            do
                printf("file %s %u\n", fd.cFileName, (unsigned)SFileVerifyFile(hMpq, fd.cFileName, SFILE_VERIFY_ALL));
            while(SFileFindNextFile(hFind, &fd));
            SFileFindClose(hFind);
        }
    }
    else
        rc = 2;

    SFileCloseArchive(hMpq);
    return rc;
}
