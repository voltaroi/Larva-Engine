#include "Sound.h"

void Sound::checkALError() {
    ALenum error = alGetError();
    if (error != AL_NO_ERROR) {
        std::cerr << "OpenAL Error: " << error << std::endl;
    }
}

ALuint Sound::load(const char* filename) {
    SF_INFO sfInfo;
    SNDFILE* file = sf_open(filename, SFM_READ, &sfInfo);

    if (!file) {
        std::cerr << "Failed to open audio file: " << filename << std::endl;
        return 0;
    }

    std::vector<short> buffer(sfInfo.frames * sfInfo.channels);
    sf_read_short(file, buffer.data(), sfInfo.frames * sfInfo.channels);
    sf_close(file);

    ALuint alBuffer;
    alGenBuffers(1, &alBuffer);
    checkALError();

    ALenum format = (sfInfo.channels == 1) ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;
    alBufferData(alBuffer, format, buffer.data(), buffer.size() * sizeof(short), sfInfo.samplerate);
    checkALError();

    return alBuffer;
}

void Sound::play(const char* filename, float x, float y, float z)
{
    std::thread([this, filename, x, y, z]() {
        ALCdevice* device = alcOpenDevice(nullptr);
        if (!device) {
            std::cerr << "Failed to open OpenAL device." << std::endl;
            return;
        }

        ALCcontext* context = alcCreateContext(device, nullptr);
        if (!alcMakeContextCurrent(context)) {
            std::cerr << "Failed to make OpenAL context current." << std::endl;
            return;
        }

        ALuint buffer = load(filename);
        if (buffer == 0) {
            return;
        }

        alGenSources(1, &source);
        checkALError();

        alSourcei(source, AL_BUFFER, buffer);
        checkALError();

        alSourcef(source, AL_GAIN, volumeMulti);
        checkALError();

        sourcePosition[0] = x;
        sourcePosition[1] = y;
        sourcePosition[2] = z;

        alSource3f(source, AL_POSITION, x, y, z);
        checkALError();

        alSourcePlay(source);
        checkALError();

        ALint state;
        do {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            alGetSourcei(source, AL_SOURCE_STATE, &state);
        } while (state == AL_PLAYING);

        alDeleteSources(1, &source);
        alDeleteBuffers(1, &buffer);
        alcDestroyContext(context);
        alcCloseDevice(device);
        }).detach();
}


void Sound::updateListenerPosition(Camera &camera)
{
    float camX = camera.getXPosition();
    float camY = camera.getYPosition();
    float camZ = camera.getZPosition();

    float listenerFrontX = camera.getFrontX();
    float listenerFrontY = camera.getFrontY();
    float listenerFrontZ = camera.getFrontZ();

    alListener3f(AL_POSITION, camX, camY, camZ);

    alListenerfv(AL_ORIENTATION, new GLfloat[6]{ listenerFrontX, listenerFrontY, listenerFrontZ, 0.0f, 1.0f, 0.0f });
    checkALError();

    float dx = sourcePosition[0] - camX;
    float dy = sourcePosition[1] - camY;
    float dz = sourcePosition[2] - camZ;
    float dist = sqrt(dx * dx + dy * dy + dz * dz);
    float volume = std::max(0.0f, 1.0f - dist / maxDistance);
    volume *= volumeMulti;

    alSourcef(source, AL_GAIN, volume);
    checkALError();

    alSource3f(source, AL_DIRECTION, listenerFrontX, listenerFrontY, listenerFrontZ);
    checkALError();
}

void Sound::setVolumeMulti(float num)
{
    volumeMulti = num;
}

void Sound::setMaxDistance(float num)
{
    maxDistance = num;
}
