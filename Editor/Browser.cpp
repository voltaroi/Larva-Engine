#include <GL/glew.h>
#include "Browser.h"
#include "EditorUI.h"
#include "Engine/Scene/Level.h"
#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

void Browser::init(const std::string &root)
{
    rootDir = root;
    showProjects();
}

void Browser::showProjects()
{
    screen = Screen::Projects;
    projects = project::listProjects(rootDir);
    newName.clear();
    confirmDelete = -1;
}

bool Browser::showLevels(const std::string &name)
{
    std::string projectName = name; // copie : name peut désigner un élément de projects, rechargé juste après
    projects = project::listProjects(rootDir);
    for (const auto &p : projects)
    {
        if (p.name == projectName)
        {
            current = p;
            screen = Screen::Levels;
            levels = project::listLevels(current.assetsDir);
            newName.clear();
            confirmDelete = -1;
            return true;
        }
    }
    setMessage("Project not found: " + projectName, true);
    return false;
}

void Browser::setMessage(const std::string &text, bool error)
{
    message = text;
    messageIsError = error;
}

Browser::Result Browser::frame(int w, int h)
{
    ui::rect(0, 0, (float)w, (float)h, ui::col::background);
    return screen == Screen::Projects ? drawProjects((float)w, (float)h) : drawLevels((float)w, (float)h);
}

void Browser::drawHeader(const std::string &title, const std::string &subtitle, float w)
{
    ui::rect(0, 0, w, 96, ui::col::panel);
    ui::rect(0, 96, w, 1, ui::col::border);
    ui::rect(40, 30, 6, 40, ui::col::accent);
    ui::text(title, 60, 26, ui::col::text, ui::Font::Title);
    ui::text(subtitle, 62, 62, ui::col::textDim);
}

void Browser::drawMessage(float x, float y)
{
    if (!message.empty())
        ui::text(message, x, y, messageIsError ? ui::col::warning : ui::col::success);
}

Browser::Result Browser::drawProjects(float w, float h)
{
    drawHeader("LARVA ENGINE  |  Level Editor", "Select a project to open", w);

    float margin = 40.0f;
    float sideW = 320.0f;
    float gridW = w - margin * 3 - sideW;

    // Cartes des projets
    ui::text("PROJECTS", margin, 120, ui::col::textDim, ui::Font::Small);
    const float cardW = 230.0f, cardH = 120.0f, gap = 16.0f;
    int perRow = std::max(1, (int)((gridW + gap) / (cardW + gap)));
    for (int i = 0; i < (int)projects.size(); i++)
    {
        const project::Info &p = projects[i];
        float cx = margin + (i % perRow) * (cardW + gap);
        float cy = 144 + (i / perRow) * (cardH + gap);
        bool hov = ui::hover(cx, cy, cardW, cardH);
        ui::rect(cx, cy, cardW, cardH, hov ? ui::col::widgetHover : ui::col::panel);
        ui::rect(cx, cy, cardW, 4, hov ? ui::col::accentHover : ui::col::accent);
        ui::rectOutline(cx, cy, cardW, cardH, hov ? ui::col::accent : ui::col::border);
        ui::textClipped(p.name, cx + 16, cy + 22, cardW - 32, ui::col::text, ui::Font::Large);
        std::string count = std::to_string(p.levelCount) + (p.levelCount == 1 ? " level" : " levels");
        ui::text(count, cx + 16, cy + 56, p.levelCount ? ui::col::text : ui::col::textDim);
        ui::textClipped("projects/" + p.name, cx + 16, cy + 86, cardW - 32, ui::col::textDim, ui::Font::Small);
        if (ui::clicked(cx, cy, cardW, cardH))
        {
            message.clear();
            showLevels(p.name);
            return Result::None;
        }
    }
    if (projects.empty())
        ui::text("No project in " + rootDir + "/projects", margin, 150, ui::col::textDim);

    // Création d'un projet
    float sx = w - margin - sideW, sy = 144;
    ui::rect(sx, sy, sideW, 190, ui::col::panel);
    ui::rectOutline(sx, sy, sideW, 190, ui::col::border);
    ui::text("New project", sx + 16, sy + 14, ui::col::text, ui::Font::Large);
    ui::text("Runs create_project.bat", sx + 16, sy + 44, ui::col::textDim, ui::Font::Small);
    ui::textField("browser.newproject", newName, sx + 16, sy + 72, sideW - 32, 28, "project_name");
    if (ui::button("Create project", sx + 16, sy + 112, sideW - 32, 32, true))
    {
        std::string msg;
        bool ok = project::createProject(rootDir, newName, msg);
        setMessage(msg, !ok);
        if (ok)
        {
            std::string name = newName;
            projects = project::listProjects(rootDir);
            showLevels(name);
            return Result::None;
        }
    }
    drawMessage(sx + 16, sy + 158);

    ui::text("Engine: " + rootDir, margin, h - 30, ui::col::textDisabled, ui::Font::Small);
    return Result::None;
}

Browser::Result Browser::drawLevels(float w, float h)
{
    drawHeader(current.name, "Levels in projects/" + current.name + "/Assets/Levels", w);
    if (ui::button("< Projects", w - 160, 34, 120, 30))
    {
        message.clear();
        showProjects();
        return Result::None;
    }

    float margin = 40.0f, sideW = 320.0f;
    float listW = w - margin * 3 - sideW;
    ui::text("LEVELS", margin, 120, ui::col::textDim, ui::Font::Small);

    float rowH = 52.0f;
    float listY = 144, listH = h - listY - 60;
    float offset = ui::scrollArea("browser.levels", margin, listY, listW, listH, levels.size() * (rowH + 6));
    ui::pushClip(margin, listY, listW - 10, listH);
    Result result = Result::None;
    for (int i = 0; i < (int)levels.size(); i++)
    {
        const std::string &path = levels[i];
        float ry = listY + i * (rowH + 6) - offset;
        float rw = listW - 14;
        bool hov = ui::hover(margin, ry, rw, rowH);
        ui::rect(margin, ry, rw, rowH, hov ? ui::col::widgetHover : ui::col::panel);
        ui::rect(margin, ry, 4, rowH, ui::col::accent);
        std::string name = fs::path(path).stem().string();
        ui::text(name, margin + 18, ry + 8, ui::col::text, ui::Font::Large);
        ui::text(path, margin + 18, ry + 32, ui::col::textDim, ui::Font::Small);

        float bx = margin + rw - 100;
        if (ui::button("Open", bx, ry + 11, 88, 30, true))
        {
            openPath = path;
            result = Result::OpenLevel;
        }
        bx -= 96;
        if (ui::button("Duplicate", bx, ry + 11, 88, 30))
        {
            Level lvl;
            std::string base = name + "_copy";
            std::string target = base;
            std::error_code ec;
            for (int n = 2; fs::exists(fs::path(current.assetsDir) / "Levels" / (target + ".lvl"), ec); n++)
                target = base + std::to_string(n);
            if (lvl.loadFromFile(current.assetsDir + "/" + path))
            {
                lvl.name = target;
                lvl.saveToFile(current.assetsDir + "/Levels/" + target + ".lvl");
                setMessage("Created " + target, false);
            }
            else
                setMessage(lvl.lastError(), true);
            levels = project::listLevels(current.assetsDir);
            break;
        }
        bx -= 96;
        if (ui::button(confirmDelete == i ? "Confirm ?" : "Delete", bx, ry + 11, 88, 30, confirmDelete == i))
        {
            if (confirmDelete == i)
            {
                std::error_code ec;
                fs::remove(fs::path(current.assetsDir) / path, ec);
                setMessage(ec ? "Cannot delete " + path : "Deleted " + path, (bool)ec);
                levels = project::listLevels(current.assetsDir);
                confirmDelete = -1;
                break;
            }
            confirmDelete = i;
        }
        // Clic sur la ligne : ouvrir
        if (ui::clicked(margin, ry, bx - margin, rowH))
        {
            openPath = path;
            result = Result::OpenLevel;
        }
    }
    if (levels.empty())
        ui::text("No level yet: create one on the right.", margin + 4, listY + 8, ui::col::textDim);
    ui::popClip();

    // Nouveau niveau
    float sx = w - margin - sideW, sy = 144;
    ui::rect(sx, sy, sideW, 190, ui::col::panel);
    ui::rectOutline(sx, sy, sideW, 190, ui::col::border);
    ui::text("New level", sx + 16, sy + 14, ui::col::text, ui::Font::Large);
    ui::text("Floor, cube and player start", sx + 16, sy + 44, ui::col::textDim, ui::Font::Small);
    ui::textField("browser.newlevel", newName, sx + 16, sy + 72, sideW - 32, 28, "Level_name");
    if (ui::button("Create and open", sx + 16, sy + 112, sideW - 32, 32, true))
    {
        std::string msg, rel;
        bool ok = project::createLevel(current.assetsDir, newName, rel, msg);
        setMessage(msg, !ok);
        levels = project::listLevels(current.assetsDir);
        if (ok)
        {
            newName.clear();
            openPath = rel;
            result = Result::OpenLevel;
        }
    }
    drawMessage(sx + 16, sy + 158);

    float hy = sy + 210;
    ui::text("IN GAME", sx, hy, ui::col::textDim, ui::Font::Small);
    const char *help[] = {
        "Levels are packed in game.pak",
        "by build_client.bat.",
        "",
        "LevelScene scene;",
        "scene.load(\"Levels/Name.lvl\");",
        "scene.render();",
    };
    for (int i = 0; i < 6; i++)
        ui::text(help[i], sx, hy + 22 + i * 20, i >= 3 ? ui::col::text : ui::col::textDim);

    if (ui::mousePressed(0) && confirmDelete >= 0 && !ui::hover(margin, listY, listW, listH))
        confirmDelete = -1;
    return result;
}
