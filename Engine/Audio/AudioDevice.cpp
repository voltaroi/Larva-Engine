#include "AudioDevice.h"
#include <AL/al.h>
#include <AL/alc.h>
#include <algorithm>
#include <iostream>

bool AudioDevice::init(int oneShotVoices)
{
    if (device)
        return true;
    ALCdevice *dev = alcOpenDevice(nullptr);
    if (!dev)
    {
        std::cerr << "[Audio] Aucun peripherique audio" << std::endl;
        return false;
    }
    ALCcontext *ctx = alcCreateContext(dev, nullptr);
    if (!ctx || !alcMakeContextCurrent(ctx))
    {
        std::cerr << "[Audio] Impossible de creer le contexte OpenAL" << std::endl;
        if (ctx)
            alcDestroyContext(ctx);
        alcCloseDevice(dev);
        return false;
    }
    device = dev;
    context = ctx;
    alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);

    oneShots.resize(std::max(1, oneShotVoices));
    alGenSources((ALsizei)oneShots.size(), oneShots.data());
    return true;
}

void AudioDevice::shutdown()
{
    if (!device)
        return;
    for (ALuint s : oneShots)
        alSourceStop(s);
    for (ALuint s : sources)
        alSourceStop(s);
    if (!oneShots.empty())
        alDeleteSources((ALsizei)oneShots.size(), oneShots.data());
    if (!sources.empty())
        alDeleteSources((ALsizei)sources.size(), sources.data());
    if (!buffers.empty())
        alDeleteBuffers((ALsizei)buffers.size(), buffers.data());
    oneShots.clear();
    sources.clear();
    buffers.clear();
    alcMakeContextCurrent(nullptr);
    alcDestroyContext((ALCcontext *)context);
    alcCloseDevice((ALCdevice *)device);
    device = context = nullptr;
}

AudioDevice::~AudioDevice()
{
    shutdown();
}

void AudioDevice::setDoppler(float factor, float speedOfSound)
{
    if (!device)
        return;
    alDopplerFactor(factor);
    alSpeedOfSound(speedOfSound);
}

unsigned int AudioDevice::createBuffer(const std::vector<float> &samples, int sampleRate)
{
    if (!device)
        return 0;
    std::vector<short> pcm(samples.size());
    for (size_t i = 0; i < samples.size(); ++i)
        pcm[i] = (short)std::max(-32767.0f, std::min(32767.0f, samples[i] * 32767.0f));
    ALuint buffer = 0;
    alGenBuffers(1, &buffer);
    alBufferData(buffer, AL_FORMAT_MONO16, pcm.data(), (ALsizei)(pcm.size() * sizeof(short)), sampleRate);
    buffers.push_back(buffer);
    return buffer;
}

unsigned int AudioDevice::createLoopSource(unsigned int buffer, float referenceDistance, float maxDistance, float rolloff)
{
    if (!device)
        return 0;
    ALuint source = 0;
    alGenSources(1, &source);
    alSourcei(source, AL_BUFFER, buffer);
    alSourcei(source, AL_LOOPING, AL_TRUE);
    alSourcef(source, AL_REFERENCE_DISTANCE, referenceDistance);
    alSourcef(source, AL_ROLLOFF_FACTOR, rolloff);
    alSourcef(source, AL_MAX_DISTANCE, maxDistance);
    alSourcef(source, AL_GAIN, 0.0f);
    alSourcePlay(source);
    sources.push_back(source);
    return source;
}

void AudioDevice::playOneShot(unsigned int buffer, float x, float y, float z, float gain, float pitch, bool relative,
                              float referenceDistance)
{
    if (!device || oneShots.empty())
        return;
    ALuint s = oneShots[nextOneShot];
    nextOneShot = (nextOneShot + 1) % (int)oneShots.size();
    alSourceStop(s);
    alSourcei(s, AL_BUFFER, buffer);
    alSourcei(s, AL_SOURCE_RELATIVE, relative ? AL_TRUE : AL_FALSE);
    alSource3f(s, AL_POSITION, x, y, z);
    alSourcef(s, AL_REFERENCE_DISTANCE, referenceDistance);
    alSourcef(s, AL_GAIN, gain);
    alSourcef(s, AL_PITCH, pitch);
    alSourcePlay(s);
}

void AudioDevice::setListener(float x, float y, float z, float fx, float fy, float fz, float vx, float vy, float vz)
{
    if (!device)
        return;
    alListener3f(AL_POSITION, x, y, z);
    alListener3f(AL_VELOCITY, vx, vy, vz);
    float orientation[6] = {fx, fy, fz, 0.0f, 1.0f, 0.0f};
    alListenerfv(AL_ORIENTATION, orientation);
}

void AudioDevice::setListenerGain(float gain)
{
    if (device)
        alListenerf(AL_GAIN, gain);
}

void AudioDevice::setGain(unsigned int source, float gain) { alSourcef(source, AL_GAIN, gain); }
void AudioDevice::setPitch(unsigned int source, float pitch) { alSourcef(source, AL_PITCH, pitch); }
void AudioDevice::setPosition(unsigned int source, float x, float y, float z) { alSource3f(source, AL_POSITION, x, y, z); }
void AudioDevice::setVelocity(unsigned int source, float vx, float vy, float vz) { alSource3f(source, AL_VELOCITY, vx, vy, vz); }
