#pragma once
#ifndef __AUDIO_DEVICE__
#define __AUDIO_DEVICE__
#include <vector>

// Périphérique OpenAL : contexte, buffers créés depuis des échantillons (ex. sons synthétisés),
// sources 3D en boucle et une petite réserve de sources pour les sons ponctuels.
// Si aucun périphérique n'est disponible, toutes les fonctions deviennent sans effet.
class AudioDevice
{
public:
    static const int SAMPLE_RATE = 44100;

    ~AudioDevice();
    bool init(int oneShotVoices = 8);
    void shutdown();
    bool isReady() const { return device != nullptr; }

    // Effet Doppler et vitesse du son (m/s)
    void setDoppler(float factor, float speedOfSound = 343.3f);

    // Buffer mono 16 bits depuis des échantillons dans [-1, 1]
    unsigned int createBuffer(const std::vector<float> &samples, int sampleRate = SAMPLE_RATE);
    // Source 3D qui joue le buffer en boucle, volume nul au départ
    unsigned int createLoopSource(unsigned int buffer, float referenceDistance = 8.0f, float maxDistance = 250.0f,
                                  float rolloff = 1.0f);

    // Son ponctuel ; relative = position relative à l'auditeur (ex. interface, entendu partout pareil)
    void playOneShot(unsigned int buffer, float x, float y, float z, float gain, float pitch = 1.0f,
                     bool relative = false, float referenceDistance = 8.0f);

    void setListener(float x, float y, float z, float forwardX, float forwardY, float forwardZ,
                     float vx = 0.0f, float vy = 0.0f, float vz = 0.0f);
    void setListenerGain(float gain);

    // Réglages rapides d'une source
    static void setGain(unsigned int source, float gain);
    static void setPitch(unsigned int source, float pitch);
    static void setPosition(unsigned int source, float x, float y, float z);
    static void setVelocity(unsigned int source, float vx, float vy, float vz);

private:
    void *device = nullptr;
    void *context = nullptr;
    std::vector<unsigned int> oneShots;
    std::vector<unsigned int> buffers;
    std::vector<unsigned int> sources;
    int nextOneShot = 0;
};

#endif
