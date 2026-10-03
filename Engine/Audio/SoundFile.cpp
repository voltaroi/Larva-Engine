#include "SoundFile.h"
#include <sndfile.h>

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
