#pragma once
#ifndef __MIC_CAPTURE__
#define __MIC_CAPTURE__
#include <string>
#include <vector>

// Capture du micro par défaut (OpenAL), en mono 16 bits. Prévu pour un push-to-talk :
// start() au début, update() à chaque image pour vider le tampon d'OpenAL, stop() à la fin, puis samples().
// Sans micro, open() renvoie false et le reste devient sans effet.
class MicCapture
{
public:
    ~MicCapture();
    // Micros disponibles (noms OpenAL) et micro par défaut du système
    static std::vector<std::string> listDevices();
    static std::string defaultDevice();

    // deviceName vide : micro par défaut
    bool open(int sampleRate = 16000, const std::string &deviceName = "");
    void close();
    bool isOpen() const { return device != nullptr; }
    // Nom du micro ouvert (vide si fermé)
    const std::string &deviceName() const { return openedName; }
    int sampleRate() const { return rate; }

    void start();
    void update();
    void stop();
    bool isRecording() const { return recording; }

    const std::vector<short> &samples() const { return captured; }
    float seconds() const { return rate > 0 ? (float)captured.size() / rate : 0.0f; }
    // Niveau du dernier morceau reçu, 0..1 (vumètre)
    float level() const { return lastLevel; }
    // Oublie les échantillons reçus (ex. vumètre de test : seul le niveau compte)
    void discard() { captured.clear(); }

    // Échantillons enregistrés sous forme de fichier WAV (pour un service de reconnaissance vocale)
    std::string toWav() const;

private:
    void *device = nullptr;
    int rate = 16000;
    bool recording = false;
    float lastLevel = 0.0f;
    std::vector<short> captured;
    std::string openedName;
};

#endif
