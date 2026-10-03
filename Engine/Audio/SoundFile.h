#pragma once
#ifndef __SOUND_FILE__
#define __SOUND_FILE__
#include <string>
#include <vector>

// Lecture d'un fichier audio (wav, ogg, flac, mp3 selon la version de libsndfile)
namespace SoundFile
{
    // Échantillons entrelacés dans [-1, 1] ; false si le fichier est absent ou illisible
    bool load(const std::string &path, std::vector<float> &samples, int &sampleRate, int &channels);
}

#endif
