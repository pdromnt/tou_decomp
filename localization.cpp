#include "localization.h"

#include <fstream>
#include <map>
#include <string>

#include <nlohmann/json.hpp>

#include "tou.h"

using nlohmann::json;

namespace {

struct LanguageInfo {
    const char *code;
    const char *name;
};

const LanguageInfo LANGUAGES[] = {
    {"en", "English"},
    {"es", "Espa\xC3\xB1ol"},
    {"pt-BR", "Portugu\xC3\xAAs (Brasil)"},
    {"fi", "Suomi"}
};

typedef std::map<std::string, std::string> StringMap;
StringMap g_English;
StringMap g_Selected;
std::map<int, std::string> g_MenuKeys;
std::string g_CurrentLanguage = "en";
char **g_BoundMenuStrings = NULL;
size_t g_BoundMenuCount = 0;

bool IsDynamicMenuSlot(int index)
{
    return index == 0x65 || (index >= 0x71 && index <= 0x8B) ||
           (index >= 0x149 && index <= 0x14C) ||
           index == 0x15E || index == 0x15F || index == 0x160 ||
           index == 0x162 || index == 0x164;
}

bool LoadCatalog(const char *code, StringMap &strings)
{
    const char *filename = std::string(code) == "pt-BR" ? "pt-br" : code;
    const std::string path = std::string("lang/") + filename + ".json";
    std::ifstream input(path.c_str(), std::ios::binary);
    if (!input.is_open()) {
        LOG("[I18N] Could not open %s\n", path.c_str());
        return false;
    }

    try {
        json root;
        input >> root;
        if (!root.is_object() || !root.contains("strings") ||
            !root["strings"].is_object()) {
            LOG("[I18N] Invalid catalog structure: %s\n", path.c_str());
            return false;
        }
        strings.clear();
        const json &values = root["strings"];
        for (json::const_iterator it = values.begin(); it != values.end(); ++it) {
            if (it.value().is_string())
                strings[it.key()] = it.value().get<std::string>();
        }
        LOG("[I18N] Loaded %s (%u strings)\n", path.c_str(),
            (unsigned int)strings.size());
        return true;
    } catch (const std::exception &error) {
        LOG("[I18N] Invalid catalog %s: %s\n", path.c_str(), error.what());
        return false;
    }
}

void BuildMenuKeyIndex(void)
{
    g_MenuKeys.clear();
    for (StringMap::const_iterator it = g_English.begin(); it != g_English.end(); ++it) {
        const std::string &key = it->first;
        if (key.compare(0, 5, "menu.") != 0) continue;
        const size_t end = key.find('.', 5);
        if (end == std::string::npos) continue;
        const std::string number = key.substr(5, end - 5);
        char *tail = NULL;
        const long index = strtol(number.c_str(), &tail, 10);
        if (tail && *tail == '\0' && index >= 0 && index < MENU_STRING_CAPACITY)
            g_MenuKeys[(int)index] = key;
    }
}

} /* namespace */

bool Localization_Init(const char *language)
{
    if (!LoadCatalog("en", g_English)) return false;
    BuildMenuKeyIndex();
    return Localization_SetLanguage(language);
}

bool Localization_SetLanguage(const char *language)
{
    int index = 0;
    for (int i = 0; i < Localization_GetLanguageCount(); ++i) {
        if (language && std::string(language) == LANGUAGES[i].code) {
            index = i;
            break;
        }
    }

    StringMap selected;
    if (index != 0 && !LoadCatalog(LANGUAGES[index].code, selected)) {
        LOG("[I18N] Falling back to English\n");
        index = 0;
    }
    g_Selected.swap(selected);
    g_CurrentLanguage = LANGUAGES[index].code;
    if (index != 0) {
        unsigned int missing = 0;
        for (StringMap::const_iterator it = g_English.begin(); it != g_English.end(); ++it)
            if (g_Selected.find(it->first) == g_Selected.end()) ++missing;
        LOG("[I18N] %s uses English fallback for %u strings\n",
            g_CurrentLanguage.c_str(), missing);
    }
    if (g_BoundMenuStrings)
        Localization_BindLegacyMenuStrings(g_BoundMenuStrings, g_BoundMenuCount);
    return language && g_CurrentLanguage == language;
}

const char *Localization_GetLanguage(void)
{
    return g_CurrentLanguage.c_str();
}

int Localization_GetLanguageIndex(void)
{
    for (int i = 0; i < Localization_GetLanguageCount(); ++i)
        if (g_CurrentLanguage == LANGUAGES[i].code) return i;
    return 0;
}

int Localization_GetLanguageCount(void)
{
    return (int)(sizeof(LANGUAGES) / sizeof(LANGUAGES[0]));
}

const char *Localization_GetLanguageCode(int index)
{
    if (index < 0 || index >= Localization_GetLanguageCount()) index = 0;
    return LANGUAGES[index].code;
}

const char *Localization_GetLanguageName(int index)
{
    if (index < 0 || index >= Localization_GetLanguageCount()) index = 0;
    return LANGUAGES[index].name;
}

const char *Text_Get(const char *key)
{
    if (!key) return "";
    StringMap::const_iterator found = g_Selected.find(key);
    if (found != g_Selected.end()) return found->second.c_str();
    found = g_English.find(key);
    if (found != g_English.end()) return found->second.c_str();
    return key;
}

void Localization_BindLegacyMenuStrings(char **menu_strings, size_t count)
{
    g_BoundMenuStrings = menu_strings;
    g_BoundMenuCount = count;
    if (!menu_strings) return;

    for (std::map<int, std::string>::const_iterator it = g_MenuKeys.begin();
         it != g_MenuKeys.end(); ++it) {
        const int index = it->first;
        if ((size_t)index >= count || IsDynamicMenuSlot(index)) continue;
        menu_strings[index] = const_cast<char *>(Text_Get(it->second.c_str()));
    }
}

const char *Text_NextGlyph(const char *text, uint8_t *glyph)
{
    if (!glyph) return text;
    *glyph = '?';
    if (!text || !*text) return text;

    const unsigned char *p = reinterpret_cast<const unsigned char *>(text);
    /* Invalid UTF-8 falls back to the original single-byte encoding so old
     * level metadata and asset-authored names retain their Latin-1 glyphs. */
    uint32_t codepoint = p[0];
    int length = 1;
    if (p[0] < 0x80) {
        codepoint = p[0];
    } else if ((p[0] & 0xE0) == 0xC0 && (p[1] & 0xC0) == 0x80) {
        codepoint = ((p[0] & 0x1F) << 6) | (p[1] & 0x3F);
        length = 2;
    } else if ((p[0] & 0xF0) == 0xE0 && (p[1] & 0xC0) == 0x80 &&
               (p[2] & 0xC0) == 0x80) {
        codepoint = ((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) |
                    (p[2] & 0x3F);
        length = 3;
    }

    if (codepoint <= 0xFF)
        *glyph = static_cast<uint8_t>(codepoint);
    return text + length;
}
