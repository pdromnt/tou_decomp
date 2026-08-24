#ifndef TOU_LOCALIZATION_H
#define TOU_LOCALIZATION_H

#include <stddef.h>
#include <stdint.h>

bool Localization_Init(const char *language);
bool Localization_SetLanguage(const char *language);
const char *Localization_GetLanguage(void);
int Localization_GetLanguageIndex(void);
int Localization_GetLanguageCount(void);
const char *Localization_GetLanguageCode(int index);
const char *Localization_GetLanguageName(int index);
const char *Text_Get(const char *key);
void Localization_BindLegacyMenuStrings(char **menu_strings, size_t count);

/* Decode one UTF-8 codepoint and map it into TOU's 8-bit glyph table.
 * Unsupported or malformed input deliberately becomes '?'. */
const char *Text_NextGlyph(const char *text, uint8_t *glyph);

#endif
