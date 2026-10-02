#pragma once
#ifndef __CLIPBOARD__
#define __CLIPBOARD__
#include <string>

// Presse-papiers du système (texte). Sous Windows uniquement ; ailleurs, getText renvoie "" et setText ne fait rien.
namespace Clipboard
{
    std::string getText();
    bool setText(const std::string &text);
}

#endif
