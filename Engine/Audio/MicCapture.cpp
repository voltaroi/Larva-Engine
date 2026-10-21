#include "MicCapture.h"
#include <AL/al.h>
#include <AL/alc.h>
#include <cmath>
#include <cstring>
#include <iostream>

MicCapture::~MicCapture()
{
    close();
}

std::vector<std::string> MicCapture::listDevices()
{
    std::vector<std::string> names;
    // Noms collés les uns aux autres, chacun terminé par un octet nul, la liste par un second octet nul
    const ALCchar *list = alcGetString(nullptr, ALC_CAPTURE_DEVICE_SPECIFIER);
    while (list && *list)
    {
        names.push_back(list);
        list += strlen(list) + 1;
    }
    return names;
}

std::string MicCapture::defaultDevice()
{
    const ALCchar *name = alcGetString(nullptr, ALC_CAPTURE_DEFAULT_DEVICE_SPECIFIER);
    return name ? name : "";
}

bool MicCapture::open(int sampleRate, const std::string &deviceName)
{
    if (device)
        return true;
    rate = sampleRate;
    // Tampon d'une seconde côté OpenAL : update() doit être appelé plus souvent que ça
    ALCdevice *dev = alcCaptureOpenDevice(deviceName.empty() ? nullptr : deviceName.c_str(), (ALCuint)rate, AL_FORMAT_MONO16,
                                          (ALCsizei)rate);
    if (!dev)
    {
        std::cerr << "[Micro] Micro indisponible : " << (deviceName.empty() ? "micro par defaut" : deviceName) << std::endl;
        return false;
    }
    device = dev;
    const ALCchar *name = alcGetString(dev, ALC_CAPTURE_DEVICE_SPECIFIER);
    openedName = name ? name : deviceName;
    return true;
}

void MicCapture::close()
{
    if (!device)
        return;
    if (recording)
        alcCaptureStop((ALCdevice *)device);
    alcCaptureCloseDevice((ALCdevice *)device);
    device = nullptr;
    recording = false;
    openedName.clear();
}

void MicCapture::start()
{
    captured.clear();
    lastLevel = 0.0f;
    if (!device || recording)
        return;
    alcCaptureStart((ALCdevice *)device);
    recording = true;
}

void MicCapture::update()
{
    if (!device || !recording)
        return;
    ALCint available = 0;
    alcGetIntegerv((ALCdevice *)device, ALC_CAPTURE_SAMPLES, 1, &available);
    if (available <= 0)
        return;
    size_t old = captured.size();
    captured.resize(old + (size_t)available);
    alcCaptureSamples((ALCdevice *)device, captured.data() + old, available);

    double sum = 0.0;
    for (size_t i = old; i < captured.size(); ++i)
        sum += (double)captured[i] * captured[i];
    lastLevel = (float)std::sqrt(sum / available) / 32768.0f;
}

void MicCapture::stop()
{
    if (!device || !recording)
        return;
    update();
    alcCaptureStop((ALCdevice *)device);
    recording = false;
}

std::string MicCapture::toWav() const
{
    unsigned int dataSize = (unsigned int)(captured.size() * sizeof(short));
    unsigned int byteRate = (unsigned int)rate * 2;
    std::string wav(44 + dataSize, '\0');
    char *p = &wav[0];
    auto put32 = [](char *at, unsigned int v) { memcpy(at, &v, 4); };
    auto put16 = [](char *at, unsigned short v) { memcpy(at, &v, 2); };
    memcpy(p, "RIFF", 4);
    put32(p + 4, 36 + dataSize);
    memcpy(p + 8, "WAVEfmt ", 8);
    put32(p + 16, 16);
    put16(p + 20, 1); // PCM
    put16(p + 22, 1); // mono
    put32(p + 24, (unsigned int)rate);
    put32(p + 28, byteRate);
    put16(p + 32, 2);
    put16(p + 34, 16);
    memcpy(p + 36, "data", 4);
    put32(p + 40, dataSize);
    if (dataSize)
        memcpy(p + 44, captured.data(), dataSize);
    return wav;
}
