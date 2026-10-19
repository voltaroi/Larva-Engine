#pragma once
#ifndef __ONLINE_OVERLAY__
#define __ONLINE_OVERLAY__
#include <functional>
#include <string>

// Panneau d'amis Larva Online, dessiné par le jeu par-dessus son interface (l'équivalent de l'overlay de Steam) :
// connexion / création de compte, liste d'amis et demandes, inviter, rejoindre la partie d'un ami, invitations reçues.
// Ne fait rien quand le jeu passe par Steam (Online::larva() == nullptr).
//
// Dans le jeu :
//   - à la fin du dessin de l'interface (avant UIWidgets::endFrame) : OnlineOverlay::draw(largeur, hauteur)
//     dans le repère de l'interface (origine en bas à gauche, mêmes unités que UIWidgets::setMouse)
//   - clavier : if (OnlineOverlay::handleKey(key)) return;   (idem handleSpecial)
//   - souris : tant que OnlineOverlay::isOpen(), libérer le curseur et ignorer les commandes du joueur
//   - un bouton ou une touche "Amis" : OnlineOverlay::toggle()  (svc.openInviteOverlay() l'ouvre aussi)
// Police : ASCII seulement (pas d'accents dans les textes affichés).
namespace OnlineOverlay
{
    bool isOpen();
    void open();
    void close();
    void toggle();

    // Panneau (s'il est ouvert) et notification des invitations reçues
    void draw(float width, float height);
    // Touche pour le panneau ouvert (Échap ferme, Tab change de champ, Entrée valide) : true si elle est consommée
    bool handleKey(unsigned char key);
    bool handleSpecial(int key, int mods);

    // Nom affiché d'un jeu à la place de son identifiant ("rempart" -> "Dernier Rempart")
    void setGameName(const std::string &id, const std::string &name);
    // Touche rappelée dans la notification d'invitation quand la souris est capturée ("F1", "Tab"...)
    void setHotkeyHint(const std::string &hint);

    // Apparence du panneau, pour qu'il ressemble au reste du jeu. Tout ce qui n'est pas fourni garde le style
    // par défaut (panneau sombre, filet de la couleur d'accent du thème des boutons).
    struct Style
    {
        std::string title = "LARVA ONLINE";
        // Fonds : panneau d'amis, notification d'invitation, ligne d'ami ou d'invitation (highlight : invitation)
        std::function<void(float x, float y, float w, float h)> panel, toast;
        std::function<void(float x, float y, float w, float h, bool highlight)> card;
        std::function<void(const std::string &text, float x, float y, float scale)> drawTitle;
        // Autour du dessin du panneau (par exemple appliquer puis rendre le thème des boutons du jeu)
        std::function<void()> begin, end;
        float shade = 0.35f;                           // voile sur le jeu, panneau ouvert
        float text[3] = {0.95f, 0.95f, 0.95f};         // noms, texte courant
        float label[3] = {0.75f, 0.78f, 0.82f};        // intitulés des champs et des listes
        float dim[3] = {0.6f, 0.62f, 0.66f};           // aides, hors ligne
        float good[3] = {0.45f, 0.9f, 0.5f};           // en ligne, dans ta partie
        float warn[3] = {1.0f, 0.8f, 0.35f};           // demandes, invitations, partie en cours
        float bad[3] = {1.0f, 0.45f, 0.35f};           // erreurs, retirer un ami
    };
    void setStyle(const Style &style);
}

#endif
