// lz4 hc streaming api example : ring buffer
// based on previous work from takayuki matsuoka


/**************************************
 * compiler options
 **************************************/
#ifdef _msc_ver    /* visual studio */
#  define _crt_secure_no_warnings // for msvc
#  define snprintf sprintf_s
#endif

#define gcc_version (__gnuc__ * 100 + __gnuc_minor__)
#ifdef __gnuc__
#  pragma gcc diagnostic ignored "-wmissing-braces"   /* gcc bug 53119 : doesn't accept { 0 } as initializer (https://gcc.gnu.org/bugzilla/show_bug.cgi?id=53119) */
#endif


/**************************************
 * includes
 **************************************/
#include "lz4hc.h"
#include "lz4.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum {
    message_max_bytes   = 1024,
    ring_buffer_bytes   = 1024 * 8 + message_max_bytes,
    dec_buffer_bytes    = ring_buffer_bytes + message_max_bytes   // intentionally larger to test unsynchronized ring buffers
};


size_t write_int32(file* fp, int32_t i) {
    return fwrite(&i, sizeof(i), 1, fp);
}

size_t write_bin(file* fp, const void* array, int arraybytes) {
    return fwrite(array, 1, arraybytes, fp);
}

size_t read_int32(file* fp, int32_t* i) {
    return fread(i, sizeof(*i), 1, fp);
}

size_t read_bin(file* fp, void* array, int arraybytes) {
    return fread(array, 1, arraybytes, fp);
}


void test_compress(file* outfp, file* inpfp)
{
    lz4_streamhc_t lz4stream_body = { 0 };
    lz4_streamhc_t* lz4stream = &lz4stream_body;

    static char inpbuf[ring_buffer_bytes];
    int inpoffset = 0;

    for(;;)
    {
        // read random length ([1,message_max_bytes]) data to the ring buffer.
        char* const inpptr = &inpbuf[inpoffset];
        const int randomlength = (rand() % message_max_bytes) + 1;
        const int inpbytes = (int) read_bin(inpfp, inpptr, randomlength);
        if (0 == inpbytes) break;

        {
            char cmpbuf[lz4_compressbound(message_max_bytes)];
            const int cmpbytes = lz4_compresshc_continue(lz4stream, inpptr, cmpbuf, inpbytes);

            if(cmpbytes <= 0) break;
            write_int32(outfp, cmpbytes);
            write_bin(outfp, cmpbuf, cmpbytes);

            inpoffset += inpbytes;

            // wraparound the ringbuffer offset
            if(inpoffset >= ring_buffer_bytes - message_max_bytes)
                inpoffset = 0;
        }
    }

    write_int32(outfp, 0);
}


void test_decompress(file* outfp, file* inpfp)
{
    static char decbuf[dec_buffer_bytes];
    int decoffset = 0;
    lz4_streamdecode_t lz4streamdecode_body = { 0 };
    lz4_streamdecode_t* lz4streamdecode = &lz4streamdecode_body;

    for(;;)
    {
        int  cmpbytes = 0;
        char cmpbuf[lz4_compressbound(message_max_bytes)];

        {
            const size_t r0 = read_int32(inpfp, &cmpbytes);
            size_t r1;
            if(r0 != 1 || cmpbytes <= 0)
                break;

            r1 = read_bin(inpfp, cmpbuf, cmpbytes);
            if(r1 != (size_t) cmpbytes)
                break;
        }

        {
            char* const decptr = &decbuf[decoffset];
            const int decbytes = lz4_decompress_safe_continue(
                lz4streamdecode, cmpbuf, decptr, cmpbytes, message_max_bytes);
            if(decbytes <= 0)
                break;

            decoffset += decbytes;
            write_bin(outfp, decptr, decbytes);

            // wraparound the ringbuffer offset
            if(decoffset >= dec_buffer_bytes - message_max_bytes)
                decoffset = 0;
        }
    }
}


// compare 2 files content
// return 0 if identical
// return bytenb>0 if different
size_t compare(file* f0, file* f1)
{
    size_t result = 1;

    for (;;)
    {
        char b0[65536];
        char b1[65536];
        const size_t r0 = fread(b0, 1, sizeof(b0), f0);
        const size_t r1 = fread(b1, 1, sizeof(b1), f1);

        if ((r0==0) && (r1==0)) return 0;   // success

        if (r0 != r1)
        {
            size_t smallest = r0;
            if (r1<r0) smallest = r1;
            result += smallest;
            break;
        }

        if (memcmp(b0, b1, r0))
        {
            unsigned errorpos = 0;
            while ((errorpos < r0) && (b0[errorpos]==b1[errorpos])) errorpos++;
            result += errorpos;
            break;
        }

        result += sizeof(b0);
    }

    return result;
}


int main(int argc, char** argv)
{
    char inpfilename[256] = { 0 };
    char lz4filename[256] = { 0 };
    char decfilename[256] = { 0 };
    unsigned fileid = 1;
    unsigned pause = 0;


    if(argc < 2) {
        printf("please specify input filename\n");
        return 0;
    }

    if (!strcmp(argv[1], "-p")) pause = 1, fileid = 2;

    snprintf(inpfilename, 256, "%s", argv[fileid]);
    snprintf(lz4filename, 256, "%s.lz4s-%d", argv[fileid], 9);
    snprintf(decfilename, 256, "%s.lz4s-%d.dec", argv[fileid], 9);

    printf("input   = [%s]\n", inpfilename);
    printf("lz4     = [%s]\n", lz4filename);
    printf("decoded = [%s]\n", decfilename);

    // compress
    {
        file* inpfp = fopen(inpfilename, "rb");
        file* outfp = fopen(lz4filename, "wb");

        test_compress(outfp, inpfp);

        fclose(outfp);
        fclose(inpfp);
    }

    // decompress
    {
        file* inpfp = fopen(lz4filename, "rb");
        file* outfp = fopen(decfilename, "wb");

        test_decompress(outfp, inpfp);

        fclose(outfp);
        fclose(inpfp);
    }

    // verify
    {
        file* inpfp = fopen(inpfilename, "rb");
        file* decfp = fopen(decfilename, "rb");

        const size_t cmp = compare(inpfp, decfp);
        if(0 == cmp) {
            printf("verify : ok\n");
        } else {
            printf("verify : ng : error at pos %u\n", (unsigned)cmp-1);
        }

        fclose(decfp);
        fclose(inpfp);
    }

    if (pause)
    {
        printf("press enter to continue ...\n");
        getchar();
    }

    return 0;
}
