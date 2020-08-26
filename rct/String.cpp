#include "String.h"

#include <zconf.h>

#ifdef RCT_HAVE_ZLIB
#include <zlib.h>

enum { BufferSize = 1024 * 32 };
#endif

String String::compress() const
{
#ifndef RCT_HAVE_ZLIB
    assert(0 && "Rct configured without zlib support");
    return String();
#else
    if (isEmpty())
        return String();
    z_stream stream;
    memset(&stream, 0, sizeof(stream));

    if (::deflateInit(&stream, Z_BEST_COMPRESSION) != Z_OK)
        return String();

    stream.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef *>(data()));
    stream.avail_in = size();

    char buffer[BufferSize];

    String out;
    out.reserve(size() / 2);

    int error = 0;
    do {
        stream.next_out = reinterpret_cast<Bytef *>(buffer);
        stream.avail_out = sizeof(buffer);

        error = ::deflate(&stream, Z_FINISH);
        if (error != Z_OK && error != Z_STREAM_END) {
            out.clear();
            break;
        }

        const int processed = sizeof(buffer) - stream.avail_out;
        out.append(buffer, processed);
    } while (!stream.avail_out);

    deflateEnd(&stream);

    return out;
#endif
}

String String::uncompress(const char *data, size_t size)
{
#ifndef RCT_HAVE_ZLIB
    (void)data;
    (void)size;
    assert(0 && "Rct configured without zlib support");
    return String();
#else
    if (!size)
        return String();
    z_stream stream;
    memset(&stream, 0, sizeof(stream));

    if (inflateInit(&stream) != Z_OK) {
        return String();
    }

    stream.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef *>(data));
    stream.avail_in = size;

    char buffer[BufferSize];

    String out;
    out.reserve(size * 2);

    int error = 0;
    do {
        stream.next_out = reinterpret_cast<Bytef *>(buffer);
        stream.avail_out = sizeof(buffer);

        error = ::inflate(&stream, Z_SYNC_FLUSH);

        if (error != Z_OK && error != Z_STREAM_END) {
            out.clear();
            break;
        }

        const int processed = sizeof(buffer) - stream.avail_out;
        out.append(buffer, processed);
    } while (!stream.avail_out);

    inflateEnd(&stream);
    return out;
#endif
}

String String::toHex(const void *pAddressIn, size_t lSize)
{
    String ret;
    char szBuf[100];
    int lIndent = 1;
    int lOutLen, lIndex, lIndex2, lOutLen2;
    int lRelPos;
    struct {
        const unsigned char *pData;
        unsigned int lSize;
    } buf;
    const unsigned char *pTmp;
    unsigned char ucTmp;
    const unsigned char *pAddress = static_cast<const unsigned char *>(pAddressIn);

    buf.pData = pAddress;
    buf.lSize = lSize;

    while (buf.lSize > 0) {
        pTmp = buf.pData;
        lOutLen = (int)buf.lSize;
        if (lOutLen > 16)
            lOutLen = 16;

        // create a 64-character formatted output line:
        snprintf(szBuf,
                 sizeof(szBuf),
                 " >                            "
                 "                      "
                 "    %08lX",
                 static_cast<long unsigned int>(pTmp - pAddress));
        lOutLen2 = lOutLen;

        for (lIndex = 1 + lIndent, lIndex2 = 53 - 15 + lIndent, lRelPos = 0; lOutLen2; lOutLen2--, lIndex += 2, lIndex2++) {
            ucTmp = *pTmp++;

            sprintf(szBuf + lIndex, "%02X ", (unsigned short)ucTmp);
            if (!isprint(ucTmp))
                ucTmp = '.'; // nonprintable char
            szBuf[lIndex2] = ucTmp;

            if (!(++lRelPos & 3)) // extra blank after 4 bytes
            {
                lIndex++;
                szBuf[lIndex + 2] = ' ';
            }
        }

        if (!(lRelPos & 3))
            lIndex--;

        szBuf[lIndex] = '<';
        szBuf[lIndex + 1] = ' ';

        ret.append(szBuf);

        buf.pData += lOutLen;
        buf.lSize -= lOutLen;
    }
    return ret;
}
