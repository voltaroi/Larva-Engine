#include "SoundFile.h"
#include <sndfile.h>
#include <cstdio>
#include <cstring>

bool SoundFile::load(const std::string &path, std::vector<float> &samples, int &sampleRate, int &channels)
{
    SF_INFO info{};
    SNDFILE *file = sf_open(path.c_str(), SFM_READ, &info);
    if (!file)
        return false;
    if (info.frames <= 0 || info.channels <= 0)
    {
        sf_close(file);
        return false;
    }
    samples.resize((size_t)info.frames * info.channels);
    sf_count_t read = sf_readf_float(file, samples.data(), info.frames);
    sf_close(file);
    samples.resize((size_t)read * info.channels);
    sampleRate = info.samplerate;
    channels = info.channels;
    return read > 0;
}

// Lecture virtuelle de libsndfile sur un bloc mémoire
namespace
{
    struct MemoryFile
    {
        const std::string *data;
        sf_count_t pos;
    };

    sf_count_t memLength(void *user) { return (sf_count_t)((MemoryFile *)user)->data->size(); }

    sf_count_t memSeek(sf_count_t offset, int whence, void *user)
    {
        MemoryFile *m = (MemoryFile *)user;
        sf_count_t size = (sf_count_t)m->data->size();
        sf_count_t pos = whence == SEEK_SET ? offset : whence == SEEK_CUR ? m->pos + offset : size + offset;
        m->pos = pos < 0 ? 0 : (pos > size ? size : pos);
        return m->pos;
    }

    sf_count_t memRead(void *ptr, sf_count_t count, void *user)
    {
        MemoryFile *m = (MemoryFile *)user;
        sf_count_t left = (sf_count_t)m->data->size() - m->pos;
        if (count > left)
            count = left;
        memcpy(ptr, m->data->data() + m->pos, (size_t)count);
        m->pos += count;
        return count;
    }

    sf_count_t memWrite(const void *, sf_count_t, void *) { return 0; }

    sf_count_t memTell(void *user) { return ((MemoryFile *)user)->pos; }
}

bool SoundFile::loadFromMemory(const std::string &data, std::vector<float> &samples, int &sampleRate, int &channels)
{
    SF_VIRTUAL_IO io{memLength, memSeek, memRead, memWrite, memTell};
    MemoryFile mem{&data, 0};
    SF_INFO info{};
    SNDFILE *file = sf_open_virtual(&io, SFM_READ, &info, &mem);
    if (!file)
        return false;
    if (info.frames <= 0 || info.channels <= 0)
    {
        sf_close(file);
        return false;
    }
    samples.resize((size_t)info.frames * info.channels);
    sf_count_t read = sf_readf_float(file, samples.data(), info.frames);
    sf_close(file);
    samples.resize((size_t)read * info.channels);
    sampleRate = info.samplerate;
    channels = info.channels;
    return read > 0;
}
