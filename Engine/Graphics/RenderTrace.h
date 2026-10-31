#pragma once
#ifndef __RENDER_TRACE__
#define __RENDER_TRACE__

// Journal du rendu pour diagnostiquer un gel chez un joueur (pilote qui bloque la carte graphique). Fermé par défaut :
// rien n'est écrit tant que open() n'a pas été appelé. Chaque ligne est écrite sur le disque tout de suite (le PC peut
// geler juste après).
//   RenderTrace::open("render.log");  RenderTrace::glInfo();
//   RenderTrace::detail(120);          // les 120 prochaines images pas à pas
//   RenderTrace::step("ombres");       // en mode détaillé : attend la carte (glFinish), note le temps et les erreurs
//   RenderTrace::endFrame();
// La dernière ligne du fichier après un gel dit quelle étape n'est jamais revenue.
namespace RenderTrace
{
    void open(const char *path);
    void close();
    bool isOpen();
    void line(const char *fmt, ...);
    // Carte graphique, pilote, limites et extensions utiles
    void glInfo();
    // Les n prochaines images notées étape par étape
    void detail(int frames);
    bool detailed();
    void step(const char *fmt, ...);
    void endFrame();
}

#endif
