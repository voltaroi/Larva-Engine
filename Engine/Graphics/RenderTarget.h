#pragma once
#ifndef __RENDER_TARGET__
#define __RENDER_TARGET__

// Framebuffer hors écran : texture de couleur (optionnelle) + texture de profondeur.
// Sert au rendu HDR suivi d'un post-traitement, ou à copier la profondeur (particules douces).
class RenderTarget
{
public:
    enum class ColorFormat
    {
        None,    // profondeur seule
        RGBA8,
        RGBA16F  // HDR
    };

    // (Re)crée les textures si la taille ou le format change. Renvoie false si le framebuffer est incomplet.
    bool resize(int width, int height, ColorFormat format);
    void destroy();

    void bind() const;             // rendu dans la cible (et viewport à sa taille)
    static void bindDefault();     // retour à l'écran

    // Copie la profondeur de cette cible vers une autre de même taille
    void blitDepthTo(const RenderTarget &target) const;

    unsigned int framebuffer() const { return fbo; }
    unsigned int colorTexture() const { return color; }
    unsigned int depthTexture() const { return depth; }
    int getWidth() const { return width; }
    int getHeight() const { return height; }
    bool isValid() const { return fbo != 0; }

private:
    unsigned int fbo = 0, color = 0, depth = 0;
    int width = 0, height = 0;
    ColorFormat format = ColorFormat::None;
};

#endif
