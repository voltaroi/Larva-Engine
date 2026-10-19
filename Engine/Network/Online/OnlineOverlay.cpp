#include "OnlineOverlay.h"
#include <GL/glew.h>
#include <algorithm>
#include <chrono>
#include <map>
#include <vector>
#include "Engine/Graphics/TextBox.h"
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
                return {tbFriend};
            return {};
        }

        void unfocusAll()
        {
            for (TextBox *t : {tbName, tbPass, tbPass2, tbFriend})
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
                status = "Hors ligne";
            }
            const float bw = 120, bh = 38, by = y + (h - bh) / 2;
            float right = x + w - 8;
            auto btn = [&](const std::string &label, bool enabled = true) {
                right -= bw;
                bool clicked = UIWidgets::button(label, right, by, bw, bh, enabled);
                right -= 8;
                return clicked;
            };
            if (confirmRemove == f.id)
            {
                if (btn("NON"))
                    confirmRemove = 0;
                if (btn("OUI"))
                {
                    svc.removeFriend(f.id);
                    confirmRemove = 0;
                }
                status = "Retirer de tes amis ?";
                sc = style.bad;
            }
            else if (f.state == Online::Friend::RequestIn)
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
                right -= 34;
                if (UIWidgets::button("x", right, by, 34, bh))
                    confirmRemove = f.id;
                right -= 8;
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
            float textW = right - (x + 20);
            text(fit(f.name, textW, ts() * 0.72f), x + 20, y + h - 30, ts() * 0.72f, style.text);
            text(fit(status, textW, ts() * 0.55f), x + 20, y + 12, ts() * 0.55f, sc);
        }

        void drawReady(LarvaOnline &svc, float x, float y, float w, float bottom)
        {
            text(fit("Connecte : " + svc.myName(), w - 190, ts() * 0.7f), x, y + 12, ts() * 0.7f, style.text);
            if (UIWidgets::button("DECONNEXION", x + w - 180, y, 180, 42))
            {
                svc.logout();
                return;
            }
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
            for (TextBox *t : {tbName, tbPass, tbPass2, tbFriend})
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
