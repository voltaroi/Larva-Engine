#pragma once
#ifndef __GPU_TIMER__
#define __GPU_TIMER__
#include <cstdio>

// Chronométrage des passes de rendu sur la carte graphique (requêtes d'horodatage relues quelques images plus tard).
// Coupé par défaut : mark() ne fait alors rien. Les bancs d'essai l'allument et lisent les moyennes par passe.
//   GpuTimer::setEnabled(true);
//   chaque image : GpuTimer::mark("ombres"); ... GpuTimer::mark("monde"); ... GpuTimer::endFrame();
//   GpuTimer::report(stdout);
// Le temps d'une passe va de son repère au repère suivant (ou à la fin de l'image).
namespace GpuTimer
{
    void setEnabled(bool enabled);
    bool enabled();
    // Compte aussi les fragments ombrés par passe (OpenGL 4.6), pour mesurer le surdessin
    void setFragmentStats(bool enabled);
    void mark(const char *label);
    void endFrame();
    void reset();
    // Moyennes (ms) par passe depuis le dernier reset()
    void report(FILE *out);
    // Moyenne (ms) d'une passe, -1 si elle n'a pas été vue
    double average(const char *label);
}

#endif
