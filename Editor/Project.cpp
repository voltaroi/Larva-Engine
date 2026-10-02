#include "Project.h"
#include "Engine/Scene/Level.h"
#include <windows.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>

namespace fs = std::filesystem;

namespace project
{
    static bool isEngineRoot(const fs::path &p)
    {
        std::error_code ec;
        return fs::is_directory(p / "Engine", ec) && fs::is_directory(p / "projects", ec);
    }

    static std::string searchUp(fs::path p)
    {
        for (int i = 0; i < 8 && !p.empty(); i++)
        {
            if (isEngineRoot(p))
                return p.generic_string();
            fs::path parent = p.parent_path();
            if (parent == p)
                break;
            p = parent;
        }
        return "";
    }

    std::string findEngineRoot()
    {
        char exePath[MAX_PATH] = {};
        GetModuleFileNameA(nullptr, exePath, MAX_PATH);
        std::string found = searchUp(fs::path(exePath).parent_path());
        if (found.empty())
            found = searchUp(fs::current_path());
        return found;
    }

    static std::string lowerExt(const fs::path &p)
    {
        std::string ext = p.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        return ext;
    }

    static std::vector<std::string> listFiles(const std::string &assetsDir, const std::string &subDir, const std::vector<std::string> &exts)
    {
        std::vector<std::string> result;
        std::error_code ec;
        fs::path base(assetsDir);
        fs::path dir = subDir.empty() ? base : base / subDir;
        if (!fs::is_directory(dir, ec))
            return result;
        for (auto it = fs::recursive_directory_iterator(dir, ec); !ec && it != fs::recursive_directory_iterator(); it.increment(ec))
        {
            if (!it->is_regular_file(ec))
                continue;
            std::string ext = lowerExt(it->path());
            if (std::find(exts.begin(), exts.end(), ext) == exts.end())
                continue;
            result.push_back(fs::relative(it->path(), base, ec).generic_string());
        }
        std::sort(result.begin(), result.end());
        return result;
    }

    std::vector<std::string> listLevels(const std::string &assetsDir)
    {
        return listFiles(assetsDir, "Levels", {".lvl"});
    }

    std::vector<std::string> listModels(const std::string &assetsDir)
    {
        return listFiles(assetsDir, "", {".fbx", ".obj", ".gltf", ".glb", ".dae", ".3ds", ".ply", ".stl"});
    }

    std::vector<Info> listProjects(const std::string &root)
    {
        std::vector<Info> result;
        std::error_code ec;
        for (auto &entry : fs::directory_iterator(fs::path(root) / "projects", ec))
        {
            if (!entry.is_directory(ec))
                continue;
            Info info;
            info.name = entry.path().filename().string();
            info.dir = entry.path().generic_string();
            info.assetsDir = (entry.path() / "Assets").generic_string();
            info.levelCount = (int)listLevels(info.assetsDir).size();
            result.push_back(info);
        }
        std::sort(result.begin(), result.end(), [](const Info &a, const Info &b)
                  { return a.name < b.name; });
        return result;
    }

    void ensureShaders(const std::string &root, const std::string &assetsDir)
    {
        std::error_code ec;
        fs::path src = fs::path(root) / "Engine" / "Graphics" / "Shaders";
        fs::path dst = fs::path(assetsDir) / "Shaders";
        fs::create_directories(dst, ec);
        for (auto &entry : fs::directory_iterator(src, ec))
        {
            fs::path target = dst / entry.path().filename();
            if (!fs::exists(target, ec))
                fs::copy_file(entry.path(), target, ec);
        }
    }

    bool isValidName(const std::string &name)
    {
        if (name.empty() || name.size() > 48)
            return false;
        for (char c : name)
        {
            if (!(std::isalnum((unsigned char)c) || c == '_'))
                return false;
        }
        return true;
    }

    bool createProject(const std::string &root, const std::string &name, std::string &message)
    {
        if (!isValidName(name))
        {
            message = "Invalid name: use letters, digits and _";
            return false;
        }
        std::error_code ec;
        if (fs::exists(fs::path(root) / "projects" / name, ec))
        {
            message = "A project with this name already exists";
            return false;
        }

        // Lancement caché de create_project.bat depuis la racine du moteur
        // Chemin absolu : cmd ne cherche pas toujours dans le dossier courant (NoDefaultCurrentDirectoryInExePath)
        std::string script = (fs::path(root) / "create_project.bat").make_preferred().string();
        std::string cmd = "cmd.exe /c \"\"" + script + "\" " + name + "\"";
        STARTUPINFOA si = {sizeof(si)};
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        PROCESS_INFORMATION pi = {};
        std::vector<char> cmdLine(cmd.begin(), cmd.end());
        cmdLine.push_back(0);
        if (!CreateProcessA(nullptr, cmdLine.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, root.c_str(), &si, &pi))
        {
            message = "Cannot run create_project.bat";
            return false;
        }
        WaitForSingleObject(pi.hProcess, 60000);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        if (!fs::exists(fs::path(root) / "projects" / name / "Assets", ec))
        {
            message = "create_project.bat failed";
            return false;
        }
        message = "Project " + name + " created";
        return true;
    }

    bool createLevel(const std::string &assetsDir, const std::string &name, std::string &relativePath, std::string &message)
    {
        if (!isValidName(name))
        {
            message = "Invalid name: use letters, digits and _";
            return false;
        }
        std::error_code ec;
        fs::path dir = fs::path(assetsDir) / "Levels";
        fs::create_directories(dir, ec);
        fs::path file = dir / (name + ".lvl");
        if (fs::exists(file, ec))
        {
            message = "A level with this name already exists";
            return false;
        }

        Level level;
        level.name = name;

        LevelEntity floor;
        floor.name = "Floor";
        floor.model = "Models/cube.fbx";
        floor.position[1] = -1.0f;
        floor.scale[0] = 20.0f;
        floor.scale[2] = 20.0f;
        floor.collision = true;
        floor.useColor = true;
        floor.color[0] = floor.color[1] = floor.color[2] = 0.6f;
        level.entities.push_back(floor);

        LevelEntity cube;
        cube.name = "Cube";
        cube.model = "Models/cube.fbx";
        cube.position[1] = 1.0f;
        cube.collision = true;
        level.entities.push_back(cube);

        LevelEntity start;
        start.name = "PlayerStart";
        start.type = LevelEntity::Type::PlayerStart;
        start.position[1] = 1.0f;
        start.position[2] = -6.0f;
        level.entities.push_back(start);

        if (!level.saveToFile(file.string()))
        {
            message = "Cannot write " + file.generic_string();
            return false;
        }
        relativePath = "Levels/" + name + ".lvl";
        message = "Level " + name + " created";
        return true;
    }
}
