#ifndef DataFile_h
#define DataFile_h

#include <stdio.h>

#include "Path.h"
#include "Serializer.h"

class DataFile
{
public:
    DataFile(const Path &path, int version)
        : mFile(nullptr), mSizeOffset(-1), mSerializer(nullptr), mDeserializer(nullptr), mPath(path), mVersion(version)
    {}

    ~DataFile()
    {
        delete mDeserializer;
        if (mFile)
            flush();
    }

    Path path() const { return mPath; }

    bool flush()
    {
        if (!mFile)
            return false;
        const int size = ftell(mFile);
        assert(mSizeOffset != -1);
        fseek(mFile, mSizeOffset, SEEK_SET);
        operator<<(size);

        fclose(mFile);
        mFile = nullptr;
        delete mSerializer;
        mSerializer = nullptr;
        if (rename(mTempFilePath.constData(), mPath.constData())) {
            Path::rm(mTempFilePath);
            mError = String::format<128>("rename error: %d %s", errno, Rct::strerror().constData());
            return false;
        }
        return true;
    }

    enum Mode {
        Read,
        Write
    };
    String error() const { return mError; }
    bool open(Mode mode)
    {
        assert(!mFile);
        if (mode == Write) {
            if (!Path::mkdir(mPath.parentDir()))
                return false;
            mTempFilePath = mPath + "XXXXXX";
            const int ret = mkstemp(&mTempFilePath[0]);
            if (ret == -1) {
                mError = String::format<128>("mkstemp failure %d (%s)", errno, Rct::strerror().constData());
                return false;
            }
            mFile = fdopen(ret, "w");
            if (!mFile) {
                mError = String::format<128>("fdopen failure %d (%s)", errno, Rct::strerror().constData());
                close(ret);
                return false;
            }
            mSerializer = new Serializer(mFile);
            operator<<(mVersion);
            mSizeOffset = ftell(mFile);
            operator<<(static_cast<int>(0));
            return true;
        } else {
            mContents = mPath.readAll();
            if (mContents.isEmpty()) {
                if (mPath.exists())
                    mError = "Read error " + mPath;
                return false;
            }
            mDeserializer = new Deserializer(mContents);
            int version;
            (*mDeserializer) >> version;
            if (version != mVersion) {
                mError = String::format<128>("Wrong database version. Expected %d, got %d for %s",
                                             mVersion, version, mPath.constData());
                return false;
            }
            int fs;
            (*mDeserializer) >> fs;
            if (static_cast<size_t>(fs) != mContents.size()) {
                mError = String::format<128>("%s seems to be corrupted. Size should have been %zu but was %d",
                                             mPath.constData(), mContents.size(), fs);
                return false;
            }
            return true;
        }
    }

    template <typename T> DataFile &operator<<(const T &t)
    {
        assert(mSerializer);
        (*mSerializer) << t;
        return *this;
    }
    template <typename T> DataFile &operator>>(T &t)
    {
        assert(mDeserializer);
        (*mDeserializer) >> t;
        return *this;
    }
private:
    FILE *mFile;
    int mSizeOffset;
    Serializer *mSerializer;
    Deserializer *mDeserializer;
    Path mPath, mTempFilePath;
    String mContents;
    String mError;
    const int mVersion;
};
#endif
