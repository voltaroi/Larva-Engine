#include <GL/glew.h>
#include "GpuTimer.h"
#include <cstring>
#include <string>
#include <vector>

namespace GpuTimer
{
    namespace
    {
        const int FRAMES = 5; // images en vol avant relecture
        const int MARKS = 48;
        struct Frame
        {
            GLuint queries[MARKS + 1] = {};
            GLuint fragments[MARKS] = {}; // fragments ombrés par passe (statistiques du pipeline)
            const char *labels[MARKS] = {};
            int count = 0;
            bool pending = false;
        };
        struct Stat
        {
            std::string label;
            double total = 0.0;
            int frames = 0;
            double fragments = 0.0;
        };
        bool on = false;
        bool statsOn = false;
        bool queryOpen = false;
        bool created = false;
        Frame frames[FRAMES];
        int current = 0;
        std::vector<Stat> stats;
        double frameTotal = 0.0;
        int frameCount = 0;

        Stat &statFor(const char *label)
        {
            for (Stat &s : stats)
                if (s.label == label)
                    return s;
            stats.push_back({label, 0.0, 0, 0.0});
            return stats.back();
        }

        void collect(Frame &f)
        {
            if (!f.pending || f.count == 0)
                return;
            f.pending = false;
            GLuint64 t[MARKS + 1];
            for (int i = 0; i <= f.count; ++i)
                glGetQueryObjectui64v(f.queries[i], GL_QUERY_RESULT, &t[i]);
            // Une passe qui revient plusieurs fois dans l'image est additionnée
            for (int i = 0; i < f.count; ++i)
            {
                Stat &s = statFor(f.labels[i]);
                s.total += (t[i + 1] - t[i]) / 1.0e6;
                if (statsOn)
                {
                    GLuint64 n = 0;
                    glGetQueryObjectui64v(f.fragments[i], GL_QUERY_RESULT, &n);
                    s.fragments += (double)n;
                }
            }
            for (int i = 0; i < f.count; ++i)
            {
                bool first = true;
                for (int j = 0; j < i; ++j)
                    if (std::strcmp(f.labels[j], f.labels[i]) == 0)
                        first = false;
                if (first)
                    statFor(f.labels[i]).frames++;
            }
            frameTotal += (t[f.count] - t[0]) / 1.0e6;
            frameCount++;
        }
    }

    void setEnabled(bool enabled)
    {
        on = enabled;
        if (on && !created)
        {
            for (Frame &f : frames)
            {
                glGenQueries(MARKS + 1, f.queries);
                glGenQueries(MARKS, f.fragments);
            }
            created = true;
        }
    }

    bool enabled() { return on; }

    void setFragmentStats(bool enabled) { statsOn = enabled && GLEW_VERSION_4_6; }

    void mark(const char *label)
    {
        if (!on)
            return;
        Frame &f = frames[current];
        if (f.count >= MARKS)
            return;
        if (statsOn)
        {
            if (queryOpen)
                glEndQuery(GL_FRAGMENT_SHADER_INVOCATIONS);
            glBeginQuery(GL_FRAGMENT_SHADER_INVOCATIONS, f.fragments[f.count]);
            queryOpen = true;
        }
        glQueryCounter(f.queries[f.count], GL_TIMESTAMP);
        f.labels[f.count++] = label;
    }

    void endFrame()
    {
        if (!on)
            return;
        Frame &f = frames[current];
        if (queryOpen)
        {
            glEndQuery(GL_FRAGMENT_SHADER_INVOCATIONS);
            queryOpen = false;
        }
        if (f.count > 0)
        {
            glQueryCounter(f.queries[f.count], GL_TIMESTAMP);
            f.pending = true;
        }
        current = (current + 1) % FRAMES;
        collect(frames[current]);
        frames[current].count = 0;
    }

    void reset()
    {
        stats.clear();
        frameTotal = 0.0;
        frameCount = 0;
    }

    void report(FILE *out)
    {
        if (frameCount == 0)
            return;
        std::fprintf(out, "  GPU par passe (moyenne sur %d images, total %.2f ms) :\n", frameCount, frameTotal / frameCount);
        for (const Stat &s : stats)
        {
            if (statsOn)
                std::fprintf(out, "    %-22s %6.2f ms  %7.2f M fragments\n", s.label.c_str(), s.total / frameCount,
                             s.fragments / frameCount / 1.0e6);
            else
                std::fprintf(out, "    %-22s %6.2f ms\n", s.label.c_str(), s.total / frameCount);
        }
    }

    double average(const char *label)
    {
        for (const Stat &s : stats)
            if (s.label == label)
                return frameCount ? s.total / frameCount : 0.0;
        return -1.0;
    }
}
