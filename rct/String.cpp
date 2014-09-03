#include "String.h"
#include <zlib.h>

enum { BufferSize = 1024 * 32 };
String String::compress() const
{
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
}

String String::uncompress(const char *data, int size)
{
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
}
