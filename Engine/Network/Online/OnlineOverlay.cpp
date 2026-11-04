#include "OnlineOverlay.h"
#include <GL/glew.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <map>
#include <vector>
#include "Engine/Graphics/Draw2D.h"
#include "Engine/Graphics/TextBox.h"
#include "Engine/Graphics/stb_image.h"
#include "Engine/Graphics/UI.h"
#include "Engine/Graphics/UIWidgets.h"
#include "LarvaOnline.h"
#include "OnlineProtocol.h"

namespace OnlineOverlay
{
    namespace
    {
        bool opened = false;
        bool createMode = false; // formulaire de création de compte
        std::map<std::string, std::string> gameNames;
        std::string hotkey;
        TextBox *tbName = nullptr, *tbPass = nullptr, *tbPass2 = nullptr, *tbFriend = nullptr;
        bool wasDown = false;
        int page = 0;
        Online::UserId confirmRemove = 0;
        size_t lastInviteCount = 0;
        double inviteTime = -100.0;
        std::string notice; // message local (mots de passe différents...)
        Style style;
        struct AvatarTex
        {
            int64_t ver = 0;
            GLuint id = 0;
        };
        std::map<Online::UserId, AvatarTex> avatarTex;
        Online::UserId profileId = 0; // profil ouvert (0 : liste d'amis)
        TextBox *tbBio = nullptr;
        bool bioLoaded = false;
        std::string bioSent, bioPrev;
        double bioEditAt = 0, profileAsked = -100;

        double now()
        {
            using namespace std::chrono;
            return duration<double>(steady_clock::now().time_since_epoch()).count();
        }

        float ts() { return UIWidgets::theme().textScale; }

        void text(const std::string &s, float x, float y, float scale, float r = 1, float g = 1, float b = 1, float a = 1)
        {
            UI::setColor(0, 0, 0, a * 0.6f);
            UI::renderText(s, x + 1.5f, y - 1.5f, scale);
            UI::setColor(r, g, b, a);
            UI::renderText(s, x, y, scale);
            UI::setColor(1, 1, 1, 1);
        }

        void text(const std::string &s, float x, float y, float scale, const float c[3], float a = 1)
        {
            text(s, x, y, scale, c[0], c[1], c[2], a);
        }

        // Coupe le texte à la largeur donnée
        std::string fit(std::string s, float width, float scale)
        {
            if (UI::getTextWidth(s, scale) <= width)
                return s;
            while (!s.empty() && UI::getTextWidth(s + "...", scale) > width)
                s.pop_back();
            return s + "...";
        }

        void initBoxes()
        {
            if (tbName)
                return;
            auto make = [](size_t len) {
                TextBox *t = new TextBox(0, 0, 300, 44);
                t->maxLength = len;
                return t;
            };
            tbName = make(16);
            tbPass = make(64);
            tbPass->password = true;
            tbPass2 = make(64);
            tbPass2->password = true;
            tbFriend = make(16);
            tbBio = make(140);
        }

        std::vector<TextBox *> activeBoxes(LarvaOnline &svc)
        {
            using S = LarvaOnline::State;
            if (svc.state() == S::NeedLogin || svc.state() == S::LoggingIn)
            {
                if (createMode)
                    return {tbName, tbPass, tbPass2};
                return {tbName, tbPass};
            }
            if (svc.state() == S::Ready)
            {
                if (profileId)
                    return profileId == svc.myId() ? std::vector<TextBox *>{tbBio} : std::vector<TextBox *>{};
                return {tbFriend};
            }
            return {};
        }

        void unfocusAll()
        {
            for (TextBox *t : {tbName, tbPass, tbPass2, tbFriend, tbBio})
                if (t)
                    t->setFocus(false);
        }

        std::string gameName(const std::string &id)
        {
            auto it = gameNames.find(id);
            return it == gameNames.end() ? id : it->second;
        }

        void submit(LarvaOnline &svc)
        {
            using S = LarvaOnline::State;
            notice.clear();
            if (svc.state() == S::NeedLogin)
            {
                if (createMode && tbPass->text != tbPass2->text)
                {
                    notice = "Les deux mots de passe sont differents";
                    return;
                }
                if (createMode)
                    svc.createAccount(tbName->text, tbPass->text);
                else
                    svc.login(tbName->text, tbPass->text);
                tbPass->text.clear();
                tbPass2->text.clear();
            }
            else if (svc.state() == S::Ready && profileId)
            {
                if (profileId == svc.myId() && bioLoaded && tbBio->text != bioSent)
                {
                    bioSent = tbBio->text;
                    svc.setBio(tbBio->text);
                }
                tbBio->setFocus(false);
            }
            else if (svc.state() == S::Ready && !tbFriend->text.empty())
            {
                svc.addFriend(tbFriend->text);
                tbFriend->text.clear();
            }
        }

        void field(TextBox *t, const std::string &label, float x, float y, float w)
        {
            text(label, x, y + 54, ts() * 0.62f, style.label);
            t->x = x;
            t->y = y;
            t->width = w;
            t->height = 44;
            t->draw();
        }

        // Formulaire de connexion / création de compte
        void drawLogin(LarvaOnline &svc, float x, float y, float w)
        {
            float half = (w - 10) / 2;
            if (UIWidgets::button("CONNEXION", x, y, half, 46, createMode))
            {
                createMode = false;
                notice.clear();
                svc.clearError();
            }
            if (UIWidgets::button("CREER UN COMPTE", x + half + 10, y, half, 46, !createMode))
            {
                createMode = true;
                notice.clear();
                svc.clearError();
            }
            y -= 110;
            field(tbName, "Pseudo", x, y, w);
            y -= 100;
            field(tbPass, "Mot de passe", x, y, w);
            if (createMode)
            {
                y -= 100;
                field(tbPass2, "Confirmer le mot de passe", x, y, w);
            }
            y -= 70;
            bool busy = svc.state() == LarvaOnline::State::LoggingIn;
            if (UIWidgets::button(busy ? "..." : createMode ? "CREER LE COMPTE" : "SE CONNECTER", x, y, w, 52,
                                  !busy && !tbName->text.empty() && !tbPass->text.empty()))
                submit(svc);
            y -= 40;
            std::string err = !notice.empty() ? notice : svc.lastError();
            if (!err.empty())
                text(fit(err, w, ts() * 0.6f), x, y, ts() * 0.6f, style.bad);
            y -= 40;
            text("Un seul compte pour tous les jeux et le launcher", x, y, ts() * 0.55f, style.dim);
        }

        // ---------------------------------------------------------------- photos et profils

        GLuint avatarTexture(LarvaOnline &svc, Online::UserId id, int64_t ver)
        {
            if (ver <= 0)
                return 0;
            int64_t have = 0;
            const std::string *png = svc.avatar(id, ver, have);
            AvatarTex &t = avatarTex[id];
            if (png && have == ver && t.ver != ver)
            {
                int w = 0, h = 0, comp = 0;
                unsigned char *px = stbi_load_from_memory((const unsigned char *)png->data(), (int)png->size(), &w, &h, &comp, 4);
                if (px)
                {
                    if (t.id)
                        glDeleteTextures(1, &t.id);
                    glGenTextures(1, &t.id);
                    glBindTexture(GL_TEXTURE_2D, t.id);
                    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
                    stbi_image_free(px);
                }
                t.ver = ver;
            }
            return t.id;
        }

        // Photo ronde, ou silhouette (tete + epaules) quand le joueur n'en a pas
        void drawAvatar(LarvaOnline &svc, Online::UserId id, int64_t ver, float cx, float cy, float r, bool online)
        {
            GLuint tex = avatarTexture(svc, id, ver);
            glEnable(GL_BLEND);
            if (tex)
            {
                glEnable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, tex);
                glColor4f(online ? 1.0f : 0.7f, online ? 1.0f : 0.7f, online ? 1.0f : 0.7f, 1.0f);
                glBegin(GL_TRIANGLE_FAN);
                glTexCoord2f(0.5f, 0.5f);
                glVertex2f(cx, cy);
                for (int i = 0; i <= 40; ++i)
                {
                    float a = (float)i * 6.2831853f / 40.0f;
                    glTexCoord2f(0.5f + std::cos(a) * 0.5f, 0.5f - std::sin(a) * 0.5f);
                    glVertex2f(cx + std::cos(a) * r, cy + std::sin(a) * r);
                }
                glEnd();
                glDisable(GL_TEXTURE_2D);
            }
            else
            {
                Draw2D::setColor(0.13f, 0.145f, 0.2f, 1.0f);
                Draw2D::circle(cx, cy, r, true, 36);
                Draw2D::setColor(1, 1, 1, online ? 0.95f : 0.5f);
                Draw2D::circle(cx, cy + r * 0.24f, r * 0.30f, true, 20);
                std::vector<float> pts;
                for (int i = 0; i <= 14; ++i)
                {
                    float t = 3.14159265f * (float)i / 14.0f;
                    pts.push_back(cx + std::cos(t) * r * 0.56f);
                    pts.push_back(cy - r * 0.60f + std::sin(t) * r * 0.5f);
                }
                Draw2D::polygon(pts);
            }
            UI::setColor(1, 1, 1, 1);
        }

        std::string lastSeenText(long long when)
        {
            if (when <= 0)
                return "";
            long long d = (long long)std::time(nullptr) - when;
            if (d < 0)
                d = 0;
            std::string s;
            if (d < 60)
                s = "a l'instant";
            else if (d < 3600)
                s = "il y a " + std::to_string(d / 60) + " min";
            else if (d < 86400)
                s = "il y a " + std::to_string(d / 3600) + " h";
            else if (d < 86400 * 60)
                s = "il y a " + std::to_string(d / 86400) + (d < 86400 * 2 ? " jour" : " jours");
            else
                s = "il y a longtemps";
            return " - " + s;
        }

        std::string memberSince(long long when)
        {
            if (when <= 0)
                return "Inconnu";
            static const char *months[] = {"janv.", "fevr.", "mars", "avril", "mai", "juin", "juil.", "aout", "sept.", "oct.", "nov.", "dec."};
            std::time_t t = (std::time_t)when;
            std::tm tmv = {};
            localtime_s(&tmv, &t);
            return std::to_string(tmv.tm_mday) + " " + months[tmv.tm_mon % 12] + " " + std::to_string(tmv.tm_year + 1900);
        }

        // La police ne dessine que l'ASCII : les autres caracteres deviennent '?'
        std::string asciiOnly(const std::string &s)
        {
            std::string out;
            for (size_t i = 0; i < s.size(); ++i)
            {
                unsigned char c = (unsigned char)s[i];
                if (c < 0x80)
                    out.push_back((char)c);
                else if (c >= 0xC0)
                    out.push_back('?');
            }
            return out;
        }

        std::vector<std::string> wrap(const std::string &s, float scale, float maxW)
        {
            std::vector<std::string> lines;
            std::string cur, word;
            auto flush = [&]() {
                if (word.empty())
                    return;
                std::string next = cur.empty() ? word : cur + " " + word;
                if (!cur.empty() && UI::getTextWidth(next, scale) > maxW)
                {
                    lines.push_back(cur);
                    cur = word;
                }
                else
                    cur = next;
                word.clear();
            };
            for (char c : s)
            {
                if (c == ' ')
                    flush();
                else
                    word.push_back(c);
            }
            flush();
            if (!cur.empty())
                lines.push_back(cur);
            return lines;
        }

        void centered(const std::string &s, float cx, float y, float scale, const float c[3])
        {
            text(s, cx - UI::getTextWidth(s, scale) / 2, y, scale, c);
        }

        void openProfile(LarvaOnline &svc, Online::UserId id)
        {
            profileId = id;
            bioLoaded = false;
            profileAsked = -100;
            confirmRemove = 0;
            if (tbBio)
                tbBio->setFocus(false);
            svc.clearError();
        }

        void closeProfile(LarvaOnline &svc)
        {
            if (bioLoaded && profileId == svc.myId() && tbBio && tbBio->text != bioSent)
            {
                bioSent = tbBio->text;
                svc.setBio(tbBio->text);
            }
            profileId = 0;
            confirmRemove = 0;
            if (tbBio)
                tbBio->setFocus(false);
        }

        void drawProfile(LarvaOnline &svc, float x, float y, float w, float bottom)
        {
            const bool self = profileId == svc.myId();
            const LarvaOnline::Profile *p = svc.profile(profileId);
            const double t = now();
            if (t - profileAsked > (p ? 20.0 : 2.0))
            {
                profileAsked = t;
                svc.requestProfile(profileId);
            }
            Online::Friend fr;
            bool isFriend = false;
            for (const Online::Friend &f : svc.friends())
                if (f.id == profileId)
                {
                    fr = f;
                    isFriend = true;
                }
            if (p && self && !bioLoaded && !tbBio->focused)
            {
                tbBio->text = p->bio;
                bioSent = bioPrev = p->bio;
                bioLoaded = true;
            }

            if (UIWidgets::button("RETOUR", x, y - 4, 150, 42))
            {
                closeProfile(svc);
                return;
            }
            // Retirer un ami : logo (personne barree), puis confirmation
            if (!self && isFriend)
            {
                if (UIWidgets::button("", x + w - 52, y - 4, 52, 42))
                    confirmRemove = confirmRemove == fr.id ? 0 : fr.id;
                const float bx = x + w - 52, by = y - 4;
                Draw2D::setColor(1, 1, 1, 0.9f);
                Draw2D::setLineWidth(2.2f);
                Draw2D::circle(bx + 20, by + 28, 6, false, 16);
                Draw2D::arc(bx + 20, by + 11, 11, 0.35f, 2.8f, 14);
                Draw2D::setColor(style.bad[0], style.bad[1], style.bad[2], 1.0f);
                Draw2D::line(bx + 31, by + 24, bx + 43, by + 24);
                UI::setColor(1, 1, 1, 1);
            }

            const float cx = x + w / 2, r = 58, cy = y - 4 - 42 - 16 - r;
            int64_t ver = p ? p->avatarVer : (isFriend ? fr.avatarVer : 0);
            drawAvatar(svc, profileId, ver, cx, cy, r, self || (isFriend && fr.state == Online::Friend::Online));
            std::string name = p ? p->name : (self ? svc.myName() : (isFriend ? fr.name : std::string("...")));
            centered(fit(name, w, ts() * 1.0f), cx, cy - r - 38, ts() * 1.0f, style.text);
            std::string status;
            const float *sc = style.dim;
            if (self)
            {
                status = "En ligne";
                sc = style.good;
            }
            else if (isFriend && fr.state == Online::Friend::Online)
            {
                status = fr.game == "launcher" || fr.game.empty() ? "En ligne" : "En jeu : " + gameName(fr.game);
                sc = style.good;
            }
            else if (isFriend)
                status = "Hors ligne" + lastSeenText(fr.lastSeen);
            else
                status = "Demande d'ami";
            centered(fit(status, w, ts() * 0.6f), cx, cy - r - 70, ts() * 0.6f, sc);

            float yy = cy - r - 112;
            text("A PROPOS", x, yy, ts() * 0.6f, style.label);
            yy -= 14;
            if (self)
            {
                yy -= 44;
                tbBio->x = x;
                tbBio->y = yy;
                tbBio->width = w;
                tbBio->height = 44;
                tbBio->draw();
                if (tbBio->text.empty() && !tbBio->focused)
                    text("Ajoute une description", x + 12, yy + 14, ts() * 0.6f, style.dim);
                // Enregistrement automatique : 0,8 s apres la derniere frappe
                if (tbBio->text != bioPrev)
                {
                    bioPrev = tbBio->text;
                    bioEditAt = t;
                }
                if (p && bioLoaded && tbBio->text != bioSent && tbBio->text != p->bio && t - bioEditAt > 0.8)
                {
                    bioSent = tbBio->text;
                    svc.setBio(tbBio->text);
                }
                yy -= 8;
                text("La photo se change depuis le launcher", x, yy - 14, ts() * 0.5f, style.dim);
                yy -= 26;
            }
            else
            {
                std::string bio = p ? asciiOnly(p->bio) : std::string();
                if (bio.empty())
                {
                    text(p ? "Aucune description" : "Chargement...", x, yy - 22, ts() * 0.6f, style.dim);
                    yy -= 30;
                }
                else
                {
                    int n = 0;
                    for (const std::string &line : wrap(bio, ts() * 0.64f, w))
                    {
                        if (++n > 4)
                            break;
                        yy -= 28;
                        text(line, x, yy, ts() * 0.64f, style.text);
                    }
                }
            }

            // Statistiques
            yy -= 16;
            const bool confirming = confirmRemove == profileId && !self && isFriend;
            const float cw = (w - 12) / 2, chh = 70;
            for (int i = 0; i < 2 && !confirming; ++i)
            {
                float sx = x + (float)i * (cw + 12);
                if (style.card)
                    style.card(sx, yy - chh, cw, chh, false);
                else
                    UI::drawBox(sx, yy - chh, cw, chh, 0.1f, 0.11f, 0.13f, 0.85f);
                text(i == 0 ? "Membre depuis" : "Amis", sx + 14, yy - 24, ts() * 0.5f, style.label);
                std::string v = i == 0 ? (p ? memberSince(p->since) : "...") : (p ? std::to_string(p->friendCount) : "...");
                text(fit(v, cw - 28, ts() * 0.72f), sx + 14, yy - 54, ts() * 0.72f, style.text);
            }
            if (!confirming)
                yy -= chh + 14;

            if (confirming)
            {
                const float ch = 100;
                if (style.card)
                    style.card(x, yy - ch, w, ch, true);
                else
                    UI::drawBox(x, yy - ch, w, ch, 0.25f, 0.1f, 0.1f, 0.9f);
                text(fit("Retirer " + fr.name + " de tes amis ?", w - 28, ts() * 0.66f), x + 14, yy - 34, ts() * 0.66f, style.bad);
                const float hw = (w - 28 - 10) / 2;
                if (UIWidgets::button("ANNULER", x + 14, yy - ch + 10, hw, 40))
                    confirmRemove = 0;
                if (UIWidgets::button("RETIRER", x + 14 + hw + 10, yy - ch + 10, hw, 40))
                {
                    svc.removeFriend(fr.id);
                    confirmRemove = 0;
                    closeProfile(svc);
                }
                yy -= ch + 10;
            }
            else if (!svc.lastError().empty())
                text(fit(svc.lastError(), w, ts() * 0.55f), x, yy - 10, ts() * 0.55f, style.bad);
            (void)bottom;
        }

        // Une ligne de la liste d'amis ; renvoie false si elle n'a pas pu être dessinée
        void drawFriend(LarvaOnline &svc, const Online::Friend &f, float x, float y, float w, float h)
        {
            if (style.card)
                style.card(x, y, w, h, false);
            else
                UI::drawBox(x, y, w, h, 0.1f, 0.11f, 0.13f, 0.85f);
            bool online = f.state == Online::Friend::Online;
            const float *stripe = online ? style.good : style.dim;
            UI::drawBox(x, y, 6, h, stripe[0] * (online ? 1.0f : 0.6f), stripe[1] * (online ? 1.0f : 0.6f), stripe[2] * (online ? 1.0f : 0.6f), 1.0f);
            std::string status;
            const float *sc = style.dim;
            switch (f.state)
            {
            case Online::Friend::RequestIn:
                status = "Te demande en ami";
                sc = style.warn;
                break;
            case Online::Friend::RequestOut:
                status = "Demande envoyee";
                break;
            case Online::Friend::Online:
                status = f.game == "launcher" || f.game.empty() ? "En ligne" : "En jeu : " + gameName(f.game);
                if (f.lobby && f.game != "launcher")
                    status += f.joinable ? " (partie ouverte)" : " (partie pleine)";
                sc = style.good;
                break;
            default:
                status = "Hors ligne" + lastSeenText(f.lastSeen);
            }
            const float bw = 120, bh = 38, by = y + (h - bh) / 2;
            float right = x + w - 8;
            auto btn = [&](const std::string &label, bool enabled = true) {
                right -= bw;
                bool clicked = UIWidgets::button(label, right, by, bw, bh, enabled);
                right -= 8;
                return clicked;
            };
            if (f.state == Online::Friend::RequestIn)
            {
                if (btn("REFUSER"))
                    svc.removeFriend(f.id);
                if (btn("ACCEPTER"))
                    svc.acceptFriend(f.id);
            }
            else if (f.state == Online::Friend::RequestOut)
            {
                if (btn("ANNULER"))
                    svc.removeFriend(f.id);
            }
            else
            {
                bool sameGame = online && f.game == svc.game();
                bool inMyLobby = svc.currentLobby() && svc.lobbyHasMember(f.id);
                if (svc.currentLobby() && online && !inMyLobby)
                {
                    if (btn("INVITER"))
                        svc.inviteFriend(f.id);
                }
                else if (sameGame && f.lobby && f.lobby != svc.currentLobby())
                {
                    if (btn("REJOINDRE", f.joinable))
                    {
                        svc.joinFriend(f.id);
                        close();
                    }
                }
                if (inMyLobby)
                    status = svc.directConnection(f.id) ? "Dans ta partie - connexion directe" : "Dans ta partie - par le relais";
            }
            // Clic sur la ligne (hors boutons) : ouvre le profil
            if (f.state != Online::Friend::RequestOut)
            {
                if (UIWidgets::hovered(x, y, right - x, h))
                    UI::drawBox(x, y, right - x, h, 1, 1, 1, 0.05f);
                if (UIWidgets::clicked(x, y, right - x, h))
                    openProfile(svc, f.id);
            }
            drawAvatar(svc, f.id, f.avatarVer, x + 38, y + h / 2, 24, online);
            float textW = right - (x + 76);
            text(fit(f.name, textW, ts() * 0.72f), x + 72, y + h - 30, ts() * 0.72f, style.text);
            text(fit(status, textW, ts() * 0.55f), x + 72, y + 12, ts() * 0.55f, sc);
        }

        void drawReady(LarvaOnline &svc, float x, float y, float w, float bottom)
        {
            if (profileId)
            {
                drawProfile(svc, x, y, w, bottom);
                return;
            }
            text(fit("Connecte : " + svc.myName(), w - 210, ts() * 0.7f), x, y + 12, ts() * 0.7f, style.text);
            if (UIWidgets::button("MON PROFIL", x + w - 176, y, 176, 42))
                openProfile(svc, svc.myId());
            y -= 40;
            if (svc.currentLobby())
                text("Partie en cours : " + std::to_string(svc.lobbyMemberCount()) + " joueur(s) - invite tes amis", x, y,
                     ts() * 0.58f, style.warn);
            else
                text("Heberge une partie pour inviter tes amis", x, y, ts() * 0.58f, style.dim);

            // Ajouter un ami
            y -= 100;
            field(tbFriend, "Ajouter un ami (pseudo)", x, y, w - 160);
            if (UIWidgets::button("AJOUTER", x + w - 150, y, 150, 44, !tbFriend->text.empty()))
                submit(svc);
            y -= 34;
            if (!svc.lastError().empty())
                text(fit(svc.lastError(), w, ts() * 0.55f), x, y, ts() * 0.55f, style.bad);

            // Invitations reçues
            const auto &invites = svc.invites();
            for (size_t i = 0; i < invites.size() && i < 3; ++i)
            {
                y -= 70;
                const Online::Invite &inv = invites[i];
                bool here = inv.game == svc.game();
                if (style.card)
                    style.card(x, y, w, 62, true);
                else
                    UI::drawBox(x, y, w, 62, 0.25f, 0.18f, 0.06f, 0.9f);
                std::string what = inv.fromName + " t'invite" + (here ? "" : " sur " + gameName(inv.game));
                text(fit(what, w - 270, ts() * 0.62f), x + 12, y + 22, ts() * 0.62f, style.warn);
                if (UIWidgets::button("x", x + w - 46, y + 12, 38, 38))
                {
                    svc.dismissInvite(i);
                    break;
                }
                if (here && UIWidgets::button("REJOINDRE", x + w - 196, y + 12, 140, 38))
                {
                    svc.acceptInvite(i);
                    close();
                    break;
                }
            }

            // Amis : demandes reçues, puis en ligne, puis hors ligne
            std::vector<Online::Friend> list = svc.friends();
            auto rank = [](const Online::Friend &f) {
                return f.state == Online::Friend::RequestIn ? 0 : f.state == Online::Friend::Online ? 1
                                                            : f.state == Online::Friend::RequestOut ? 2 : 3;
            };
            std::sort(list.begin(), list.end(), [&](const Online::Friend &a, const Online::Friend &b) {
                if (rank(a) != rank(b))
                    return rank(a) < rank(b);
                return OnlineProto::lower(a.name) < OnlineProto::lower(b.name);
            });
            y -= 50;
            int onlineCount = 0;
            for (const auto &f : list)
                onlineCount += f.state == Online::Friend::Online;
            text("AMIS  " + std::to_string(onlineCount) + " en ligne / " + std::to_string(list.size()), x, y + 6, ts() * 0.62f, style.label);
            const float rowH = 66, gap = 6;
            int perPage = std::max(1, (int)((y - 10 - bottom - 50) / (rowH + gap)));
            int pages = std::max(1, ((int)list.size() + perPage - 1) / perPage);
            page = std::min(page, pages - 1);
            if (pages > 1)
            {
                if (UIWidgets::arrowButton(-1, x + w - 130, y, 40, 34, page > 0))
                    page--;
                text(std::to_string(page + 1) + "/" + std::to_string(pages), x + w - 82, y + 6, ts() * 0.55f, style.text);
                if (UIWidgets::arrowButton(1, x + w - 40, y, 40, 34, page < pages - 1))
                    page++;
            }
            if (list.empty())
                text("Aucun ami pour l'instant : ajoute-les par leur pseudo", x, y - 50, ts() * 0.55f, style.dim);
            for (int i = page * perPage; i < (int)list.size() && i < (page + 1) * perPage; ++i)
            {
                y -= rowH + gap;
                drawFriend(svc, list[i], x, y, w, rowH);
            }
        }

        // Notification d'invitation, panneau fermé : quelques secondes en haut à droite
        void drawToast(LarvaOnline &svc, float width, float height)
        {
            const auto &invites = svc.invites();
            if (invites.size() > lastInviteCount)
                inviteTime = now();
            lastInviteCount = invites.size();
            if (invites.empty() || now() - inviteTime > 12.0)
                return;
            const Online::Invite &inv = invites.back();
            const float w = 520, h = 110, x = width - w - 30, y = height - h - 30;
            if (style.begin)
                style.begin();
            if (style.toast)
                style.toast(x, y, w, h);
            else
            {
                UI::drawBox(x, y, w, h, 0.08f, 0.08f, 0.1f, 0.92f);
                UI::drawBox(x, y + h - 5, w, 5, 1.0f, 0.75f, 0.25f, 1.0f);
            }
            bool here = inv.game == svc.game();
            text(fit(inv.fromName + " t'invite a jouer" + (here ? "" : " (" + gameName(inv.game) + ")"), w - 30, ts() * 0.66f), x + 16,
                 y + h - 42, ts() * 0.66f, style.warn);
            if (!hotkey.empty())
                text("Amis : " + hotkey, x + 16, y + 16, ts() * 0.55f, style.dim);
            if (here && UIWidgets::button("REJOINDRE", x + w - 300, y + 10, 140, 40))
                svc.acceptInvite(invites.size() - 1);
            else if (UIWidgets::button("IGNORER", x + w - 150, y + 10, 136, 40))
                svc.dismissInvite(invites.size() - 1);
            if (style.end)
                style.end();
        }
    }

    bool isOpen() { return opened && Online::larva(); }

    int onlineFriendCount()
    {
        LarvaOnline *s = Online::larva();
        if (!s || s->state() != LarvaOnline::State::Ready)
            return 0;
        int n = 0;
        for (const Online::Friend &f : s->friends())
            n += f.state == Online::Friend::Online;
        return n;
    }

    void open()
    {
        if (!Online::larva())
            return;
        opened = true;
        notice.clear();
        Online::larva()->clearError();
    }

    void close()
    {
        if (LarvaOnline *s = Online::larva())
            if (profileId)
                closeProfile(*s);
        opened = false;
        confirmRemove = 0;
        unfocusAll();
    }

    void toggle()
    {
        if (isOpen())
            close();
        else
            open();
    }

    void setGameName(const std::string &id, const std::string &name) { gameNames[id] = name; }
    void setHotkeyHint(const std::string &hint) { hotkey = hint; }
    void setStyle(const Style &s) { style = s; }

    static void drawPanel(LarvaOnline &svcRef, float width, float height);

    void draw(float width, float height)
    {
        LarvaOnline *svc = Online::larva();
        if (!svc)
            return;
        initBoxes();
        if (svc->takeOverlayRequest())
            open();
        if (!opened)
        {
            drawToast(*svc, width, height);
            return;
        }
        lastInviteCount = svc->invites().size();

        UIWidgets::beginModal(); // l'interface du jeu en dessous ne reçoit plus la souris
        // Clic : focus des champs de texte (au moment de l'appui, pour pouvoir glisser une sélection)
        bool down = UIWidgets::mouseDown();
        std::vector<TextBox *> boxes = activeBoxes(*svc);
        if (down && !wasDown)
            for (TextBox *t : {tbName, tbPass, tbPass2, tbFriend, tbBio})
            {
                bool active = std::find(boxes.begin(), boxes.end(), t) != boxes.end();
                if (active)
                    t->click((float)UIWidgets::mouseX(), (float)UIWidgets::mouseY());
                else
                    t->setFocus(false);
            }
        wasDown = down;

        if (style.begin)
            style.begin();
        drawPanel(*svc, width, height);
        if (style.end)
            style.end();
    }

    // Le panneau ouvert (entre style.begin et style.end)
    static void drawPanel(LarvaOnline &svcRef, float width, float height)
    {
        LarvaOnline *svc = &svcRef;
        UI::drawBox(0, 0, width, height, 0, 0, 0, style.shade);
        const float pw = std::min(640.0f, width - 40), ph = height - 60;
        const float px = width - pw - 20, py = 30;
        if (style.panel)
            style.panel(px, py, pw, ph);
        else
        {
            UI::drawBox(px, py, pw, ph, 0.05f, 0.06f, 0.07f, 1.0f);
            UI::drawBox(px, py + ph - 5, pw, 5, UIWidgets::theme().accent[0], UIWidgets::theme().accent[1], UIWidgets::theme().accent[2], 1.0f);
        }
        const float x = px + 30, w = pw - 60;
        float y = py + ph - 70;
        if (style.drawTitle)
            style.drawTitle(style.title, x, y, ts() * 1.0f);
        else
            text(style.title, x, y, ts() * 1.0f, style.text);
        if (UIWidgets::button("FERMER", px + pw - 160, y - 8, 130, 44))
        {
            close();
            return;
        }
        y -= 80;
        using S = LarvaOnline::State;
        switch (svc->state())
        {
        case S::Connecting:
            text("Connexion au serveur...", x, y, ts() * 0.7f, style.label);
            break;
        case S::Offline:
            text("Serveur Larva Online injoignable", x, y, ts() * 0.7f, style.bad);
            text("Nouvel essai automatique dans quelques secondes", x, y - 44, ts() * 0.58f, style.dim);
            break;
        case S::NeedLogin:
        case S::LoggingIn:
            drawLogin(*svc, x, y - 20, w);
            break;
        case S::Ready:
            drawReady(*svc, x, y, w, py);
            break;
        }
    }

    bool handleKey(unsigned char key)
    {
        LarvaOnline *svc = Online::larva();
        if (!svc || !opened)
            return false;
        initBoxes();
        if (key == 27)
        {
            if (profileId && svc->state() == LarvaOnline::State::Ready)
            {
                if (tbBio->focused)
                    tbBio->setFocus(false);
                else
                    closeProfile(*svc);
            }
            else
                close();
            return true;
        }
        std::vector<TextBox *> boxes = activeBoxes(*svc);
        if (key == 9 && !boxes.empty()) // Tab : champ suivant
        {
            size_t next = 0;
            for (size_t i = 0; i < boxes.size(); ++i)
                if (boxes[i]->focused)
                    next = (i + 1) % boxes.size();
            unfocusAll();
            boxes[next]->setFocus(true);
            return true;
        }
        if (key == 13)
        {
            submit(*svc);
            return true;
        }
        for (TextBox *t : boxes)
            if (t->focused)
                t->handleKey(key);
        return true;
    }

    bool handleSpecial(int key, int mods)
    {
        LarvaOnline *svc = Online::larva();
        if (!svc || !opened)
            return false;
        initBoxes();
        for (TextBox *t : activeBoxes(*svc))
            if (t->focused)
                t->handleSpecial(key, mods);
        return true;
    }
}
