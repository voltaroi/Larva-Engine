#include "UI.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define M_PI 3.14159265358979323846

std::map<unsigned char, UICharacter> UICharactersData;

float UI::textR = 1.0f;
float UI::textG = 1.0f;
float UI::textB = 1.0f;
float UI::textA = 1.0f;

void UI::setColor(float r, float g, float b, float a)
{
    textR = r;
    textG = g;
    textB = b;
    textA = a;
}

void UI::loadfont(const char *fontPath)
{
    FT_Library ft;
    if (FT_Init_FreeType(&ft))
        throw std::runtime_error("Unable to initialize FreeType");

    FT_Face face;
    if (FT_New_Face(ft, fontPath, 0, &face))
        throw std::runtime_error("Unable to load font");

    FT_Set_Pixel_Sizes(face, 0, 48);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    for (unsigned char c = 0; c < 128; c++)
    {
        if (FT_Load_Char(face, c, FT_LOAD_RENDER))
            continue;

        GLuint tex;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);

        int w = face->glyph->bitmap.width;
        int h = face->glyph->bitmap.rows;
        unsigned char *texData = new unsigned char[w * h * 4];

        for (int i = 0; i < w * h; i++)
        {
            texData[i * 4 + 0] = 255; // R
            texData[i * 4 + 1] = 255; // G
            texData[i * 4 + 2] = 255; // B
            texData[i * 4 + 3] = face->glyph->bitmap.buffer[i];
        }

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, texData);
        delete[] texData;

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        UICharacter ch = {
            tex,
            static_cast<unsigned int>(w),
            static_cast<unsigned int>(h),
            face->glyph->bitmap_left,
            face->glyph->bitmap_top,
            static_cast<unsigned int>(face->glyph->advance.x)};

        UICharactersData.insert(std::make_pair(c, ch));
    }

    FT_Done_Face(face);
    FT_Done_FreeType(ft);
}

void UI::renderText(std::string text, float x, float y, float scale)
{
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glColor4f(textR, textG, textB, textA);

    for (unsigned char c : text)
    {
        UICharacter ch = UICharactersData[c];

        float xpos = x + ch.bearingX * scale;
        float ypos = y - (ch.height - ch.bearingY) * scale;

        float w = ch.width * scale;
        float h = ch.height * scale;

        glBindTexture(GL_TEXTURE_2D, ch.textureID);

        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 1.0f);
        glVertex2f(xpos, ypos);
        glTexCoord2f(1.0f, 1.0f);
        glVertex2f(xpos + w, ypos);
        glTexCoord2f(1.0f, 0.0f);
        glVertex2f(xpos + w, ypos + h);
        glTexCoord2f(0.0f, 0.0f);
        glVertex2f(xpos, ypos + h);
        glEnd();

        x += (ch.advance >> 6) * scale;
    }

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
}

void UI::drawText(float x, float y, const char *text, void *font)
{
    glColor3f(1.0f, 1.0f, 1.0f);
    glRasterPos2f(x, y);
    while (*text)
    {
        glutBitmapCharacter(font, *text);
        ++text;
    }
}

void UI::drawProgressBar(float x, float y, float width, float height, float percentage, float r, float g, float b)
{
    glColor3f(0, 0, 0);
    glBegin(GL_LINE_LOOP);
    glVertex2f(x, y);
    glVertex2f(x + width, y);
    glVertex2f(x + width, y + height);
    glVertex2f(x, y + height);
    glEnd();

    glColor3f(r, g, b);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + width * percentage, y);
    glVertex2f(x + width * percentage, y + height);
    glVertex2f(x, y + height);
    glEnd();
}

void UI::drawBox(float x, float y, float width, float height, float r, float g, float b, float alpha, bool border, float radius)
{
    const int numSegments = 20;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(r, g, b, alpha);

    glBegin(GL_QUADS);
    glVertex2f(x + radius, y);
    glVertex2f(x + width - radius, y);
    glVertex2f(x + width - radius, y + height);
    glVertex2f(x + radius, y + height);
    glEnd();

    glBegin(GL_QUADS);
    glVertex2f(x + width, y + radius);
    glVertex2f(x + width - radius, y + radius);
    glVertex2f(x + width - radius, y + height - radius);
    glVertex2f(x + width, y + height - radius);
    glEnd();

    glBegin(GL_QUADS);
    glVertex2f(x, y + radius);
    glVertex2f(x + radius, y + radius);
    glVertex2f(x + radius, y + height - radius);
    glVertex2f(x, y + height - radius);
    glEnd();

    auto drawCorner = [&](float cx, float cy, float startAngle)
    {
        glBegin(GL_TRIANGLE_FAN);
        glVertex2f(cx, cy);
        for (int i = 0; i <= numSegments; i++)
        {
            float theta = startAngle + (i * (M_PI / 2) / numSegments);
            float dx = cos(theta) * radius;
            float dy = sin(theta) * radius;
            glVertex2f(cx + dx, cy + dy);
        }
        glEnd();
    };

    drawCorner(x + radius, y + radius, M_PI);
    drawCorner(x + width - radius, y + radius, 1.5f * M_PI);
    drawCorner(x + width - radius, y + height - radius, 0);
    drawCorner(x + radius, y + height - radius, 0.5f * M_PI);

    if (border)
    {
        glColor4f(0.0f, 0.0f, 0.0f, 1.0f);
        glLineWidth(2.0f);
        glBegin(GL_LINE_LOOP);
        for (int i = 0; i <= numSegments; i++)
        {
            float theta = i * (M_PI / 2) / numSegments;
            float dx = cos(theta) * radius;
            float dy = sin(theta) * radius;

            glVertex2f(x + radius - dx, y + radius - dy);
            glVertex2f(x + width - radius + dx, y + radius - dy);
            glVertex2f(x + width - radius + dx, y + height - radius + dy);
            glVertex2f(x + radius - dx, y + height - radius + dy);
        }
        glEnd();
    }

    glDisable(GL_BLEND);
}

void UI::drawImage(float x, float y, float width, float height, GLuint textureId, bool useOriginalSize, float opacity)
{
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, textureId);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glColor4f(1.0f, 1.0f, 1.0f, opacity);

    if (useOriginalSize)
    {
        GLint texWidth, texHeight;
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &texWidth);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &texHeight);
        float newWidth = (float)texWidth;
        float newHeight = (float)texHeight;
        width = newWidth * width;
        height = newHeight * height;
    }

    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 1.0f);
    glVertex2f(x, y);
    glTexCoord2f(1.0f, 1.0f);
    glVertex2f(x + width, y);
    glTexCoord2f(1.0f, 0.0f);
    glVertex2f(x + width, y + height);
    glTexCoord2f(0.0f, 0.0f);
    glVertex2f(x, y + height);
    glEnd();

    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
}

GLuint UI::loadTexture(const char *path, bool nearest)
{
    int width, height, channels;
    unsigned char *data = stbi_load(path, &width, &height, &channels, STBI_rgb_alpha);
    if (!data)
    {
        fprintf(stderr, "Erreur de chargement de l'image : %s\n", path);
        fprintf(stderr, "Raison : %s\n", stbi_failure_reason());
        return 0;
    }

    GLuint textureId;
    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D, textureId);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, data);

    if (nearest)
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
    else
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    stbi_image_free(data);
    return textureId;
}
