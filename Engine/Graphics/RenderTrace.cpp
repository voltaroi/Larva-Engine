#include "RenderTrace.h"
#include <GL/glew.h>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <io.h>
#include <string>

namespace RenderTrace
{
    namespace
    {
        FILE *file = nullptr;
        int detailFrames = 0, frame = 0;
        std::chrono::steady_clock::time_point start, last;

        void write(const char *fmt, va_list args)
        {
            const double t = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
            std::fprintf(file, "%9.3f  ", t);
            std::vfprintf(file, fmt, args);
            std::fputc('\n', file);
            std::fflush(file);
            _commit(_fileno(file)); // jusqu'au disque : un gel du PC ne doit pas perdre la dernière ligne
        }
    }

    void open(const char *path)
    {
        if (file)
            return;
        // Le journal d'avant est gardé (une session qui a gelé) ; trop gros, il repart de zéro
        file = std::fopen(path, "ab");
        if (file && std::ftell(file) > 2 * 1024 * 1024)
        {
            std::fclose(file);
            file = std::fopen(path, "wb");
        }
        if (!file)
            return;
        start = last = std::chrono::steady_clock::now();
        char date[64] = "?";
        const std::time_t now = std::time(nullptr);
        if (const std::tm *tm = std::localtime(&now))
            std::strftime(date, sizeof(date), "%Y-%m-%d %H:%M:%S", tm);
        line("==================== session %s ====================", date);
    }

    void close()
    {
        if (!file)
            return;
        line("==================== journal coupe ====================");
        std::fclose(file);
        file = nullptr;
        detailFrames = 0;
    }

    bool isOpen() { return file != nullptr; }

    void line(const char *fmt, ...)
    {
        if (!file)
            return;
        va_list args;
        va_start(args, fmt);
        write(fmt, args);
        va_end(args);
    }

    void glInfo()
    {
        if (!file)
            return;
        auto str = [](GLenum e) {
            const GLubyte *s = glGetString(e);
            return s ? (const char *)s : "?";
        };
        line("GL_VENDOR   %s", str(GL_VENDOR));
        line("GL_RENDERER %s", str(GL_RENDERER));
        line("GL_VERSION  %s", str(GL_VERSION));
        line("GLSL        %s", str(GL_SHADING_LANGUAGE_VERSION));
        GLint units = 0, layers = 0, size = 0, combined = 0;
        glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &units);
        glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &combined);
        glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &layers);
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &size);
        line("unites de texture %d (combinees %d), couches %d, taille max %d", units, combined, layers, size);
        line("GL 3.3 %d, 4.3 %d, ARB_copy_image %d, ARB_timer_query %d", GLEW_VERSION_3_3 ? 1 : 0, GLEW_VERSION_4_3 ? 1 : 0,
             GLEW_ARB_copy_image ? 1 : 0, GLEW_ARB_timer_query ? 1 : 0);
        while (glGetError() != GL_NO_ERROR)
            ;
    }

    void detail(int frames)
    {
        if (!file)
            return;
        detailFrames = frames;
        frame = 0;
        line("--- %d images pas a pas ---", frames);
        glFinish();
        last = std::chrono::steady_clock::now();
    }

    bool detailed() { return file && detailFrames > 0; }

    void step(const char *fmt, ...)
    {
        if (!detailed())
            return;
        char text[256];
        va_list args;
        va_start(args, fmt);
        std::vsnprintf(text, sizeof(text), fmt, args);
        va_end(args);
        // Noté avant l'attente : si la carte ne revient jamais, c'est la dernière ligne. Le temps ne compte pas l'écriture
        // du journal.
        const auto before = std::chrono::steady_clock::now();
        line("  [%d] %s ...", frame, text);
        const auto written = std::chrono::steady_clock::now();
        glFinish();
        const double ms = std::chrono::duration<double, std::milli>((before - last) + (std::chrono::steady_clock::now() - written)).count();
        std::string errors;
        for (int i = 0; i < 8; ++i)
        {
            const GLenum e = glGetError();
            if (e == GL_NO_ERROR)
                break;
            char code[16];
            std::snprintf(code, sizeof(code), " 0x%04X", (unsigned)e);
            errors += code;
        }
        line("  [%d] %s ok %.2f ms%s%s", frame, text, ms, errors.empty() ? "" : "  ERREUR GL", errors.c_str());
        last = std::chrono::steady_clock::now();
    }

    void endFrame()
    {
        if (!detailed())
            return;
        step("fin de l image");
        ++frame;
        if (--detailFrames == 0)
            line("--- fin du pas a pas ---");
    }
}
