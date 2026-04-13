// Added: Patron credits screen shown briefly when the user quits from the main menu.
#include "jkGuiPatrons.h"

#ifdef QOL_IMPROVEMENTS

#include "stdPlatform.h"
#include "jk.h"
#include "General/stdBitmap.h"
#include "Gui/jkGUI.h"
#include "Gui/jkGUIRend.h"
#include "Platform/Common/stdEmbeddedRes.h"

#include <stdlib.h>
#include <string.h>

#define JKPATRONS_MAX_GOLD    128
#define JKPATRONS_MAX_OTHER   6
#define JKPATRONS_LINE_MAX    96
#define JKPATRONS_GRACE_MS    3000

// Menu is 640x480. Font textType indices: 0=small, 2=med, 5=large.
#define JKPATRONS_FONT_TITLE  2
#define JKPATRONS_FONT_GOLD   2
#define JKPATRONS_FONT_FOOTER 0

// Element slots: 1 full-screen dismiss button + 1 title + N gold + M other + terminator.
#define JKPATRONS_MAX_ELEMENTS (2 + JKPATRONS_MAX_GOLD + JKPATRONS_MAX_OTHER + 1)

// Stored narrow — jkGui_InitMenu reads origStr and converts to wstr internally
// (see src/Gui/jkGUI.c:160-177: ELEMENT_TEXT looks up str as a translation key,
// falling back to an ASCII->wide copy if the key isn't in the string table).
static char jkGuiPatrons_sTitle[64];
static char jkGuiPatrons_sGold[JKPATRONS_MAX_GOLD][JKPATRONS_LINE_MAX];
static char jkGuiPatrons_sOther[JKPATRONS_MAX_OTHER][JKPATRONS_LINE_MAX];

static int jkGuiPatrons_numGold  = 0;
static int jkGuiPatrons_numOther = 0;
static int jkGuiPatrons_bLoaded  = 0;

static jkGuiElement jkGuiPatrons_elements[JKPATRONS_MAX_ELEMENTS];
static jkGuiMenu    jkGuiPatrons_menu;

static uint32_t jkGuiPatrons_startMs;

// Strip trailing CR/LF/whitespace and leading whitespace from a mutable buffer.
static void jkGuiPatrons_TrimInPlace(char* s)
{
    int len = 0;
    while (s[len]) len++;
    while (len > 0 && (s[len-1] == '\r' || s[len-1] == '\n' || s[len-1] == ' ' || s[len-1] == '\t'))
    {
        s[--len] = 0;
    }
    int i = 0;
    while (s[i] == ' ' || s[i] == '\t') i++;
    if (i > 0)
    {
        int j = 0;
        while (s[i]) s[j++] = s[i++];
        s[j] = 0;
    }
}

// Decode UTF-8 source into Latin-1 narrow bytes. Codepoints > 0xFF become '?'.
// The game's SFT fonts cover the Latin-1 range, and jkGui_InitMenu's
// stdString_CstrCopy zero-extends each byte to a wchar_t codepoint, so storing
// Latin-1 here yields correctly-rendered Unicode at draw time.
static void jkGuiPatrons_CopyNarrow(char* dst, int dstCap, const char* src)
{
    int o = 0;
    int i = 0;
    while (src[i] && o < dstCap - 1)
    {
        unsigned char b0 = (unsigned char)src[i];
        unsigned int cp;
        int consumed;

        if (b0 < 0x80)
        {
            cp = b0;
            consumed = 1;
        }
        else if ((b0 & 0xE0) == 0xC0)
        {
            unsigned char b1 = (unsigned char)src[i + 1];
            if ((b1 & 0xC0) != 0x80) { cp = '?'; consumed = 1; }
            else { cp = ((b0 & 0x1F) << 6) | (b1 & 0x3F); consumed = 2; }
        }
        else if ((b0 & 0xF0) == 0xE0)
        {
            unsigned char b1 = (unsigned char)src[i + 1];
            unsigned char b2 = (unsigned char)src[i + 2];
            if ((b1 & 0xC0) != 0x80 || (b2 & 0xC0) != 0x80) { cp = '?'; consumed = 1; }
            else { cp = ((b0 & 0x0F) << 12) | ((b1 & 0x3F) << 6) | (b2 & 0x3F); consumed = 3; }
        }
        else if ((b0 & 0xF8) == 0xF0)
        {
            cp = '?';
            consumed = 4;
        }
        else
        {
            cp = '?';
            consumed = 1;
        }

        dst[o++] = (cp <= 0xFF) ? (char)cp : '?';
        i += consumed;
    }
    dst[o] = 0;
}

// Process a single trimmed line: switch sections or append to current bucket.
static void jkGuiPatrons_HandleLine(char* line, int* pSection)
{
    if (line[0] == 0)    return;
    if (line[0] == '#')  return;

    if (line[0] == '[')
    {
        if (line[1] == 't')      *pSection = 0; // [title]
        else if (line[1] == 'g') *pSection = 1; // [gold]
        else if (line[1] == 'o') *pSection = 2; // [other]
        return;
    }

    if (*pSection == 0)
    {
        jkGuiPatrons_CopyNarrow(jkGuiPatrons_sTitle, 64, line);
    }
    else if (*pSection == 1 && jkGuiPatrons_numGold < JKPATRONS_MAX_GOLD)
    {
        jkGuiPatrons_CopyNarrow(jkGuiPatrons_sGold[jkGuiPatrons_numGold],
                                JKPATRONS_LINE_MAX, line);
        jkGuiPatrons_numGold++;
    }
    else if (*pSection == 2 && jkGuiPatrons_numOther < JKPATRONS_MAX_OTHER)
    {
        jkGuiPatrons_CopyNarrow(jkGuiPatrons_sOther[jkGuiPatrons_numOther],
                                JKPATRONS_LINE_MAX, line);
        jkGuiPatrons_numOther++;
    }
}

int jkGuiPatrons_Startup(const char* fpath)
{
    stdPlatform_Printf("OpenJKDF2: %s\n", __func__);

    jkGuiPatrons_numGold = 0;
    jkGuiPatrons_numOther = 0;
    jkGuiPatrons_bLoaded = 0;

    // Default title; overridable via "[title]" section.
    jkGuiPatrons_CopyNarrow(jkGuiPatrons_sTitle, 64, "Thank You To Our Gold Patrons");

    // Load via stdEmbeddedRes_Load which handles CWD resource/, SDL base path,
    // macOS bundle, Android APK assets and /sdcard/OpenJKDF2/resource/.
    size_t bufSz = 0;
    char* buf = stdEmbeddedRes_Load(fpath, &bufSz);
    if (!buf || bufSz == 0)
    {
        if (buf) free(buf);
        stdPlatform_Printf("OpenJKDF2: jkGuiPatrons: '%s' not found, patron screen disabled.\n", fpath);
        return 0;
    }

    char line[JKPATRONS_LINE_MAX];
    int lineLen = 0;
    int section = 1; // default to [gold]

    for (size_t i = 0; i < bufSz; i++)
    {
        char c = buf[i];
        if (c == 0) break; // stdEmbeddedRes null-terminates; bufSz includes the terminator
        if (c == '\n' || c == '\r')
        {
            line[lineLen] = 0;
            jkGuiPatrons_TrimInPlace(line);
            jkGuiPatrons_HandleLine(line, &section);
            lineLen = 0;
        }
        else if (lineLen < JKPATRONS_LINE_MAX - 1)
        {
            line[lineLen++] = c;
        }
    }
    if (lineLen > 0)
    {
        line[lineLen] = 0;
        jkGuiPatrons_TrimInPlace(line);
        jkGuiPatrons_HandleLine(line, &section);
    }

    free(buf);

    jkGuiPatrons_bLoaded = 1;
    stdPlatform_Printf("OpenJKDF2: jkGuiPatrons: loaded %d gold, %d other.\n",
                       jkGuiPatrons_numGold, jkGuiPatrons_numOther);
    return 1;
}

void jkGuiPatrons_Shutdown(void)
{
    stdPlatform_Printf("OpenJKDF2: %s\n", __func__);
    jkGuiPatrons_bLoaded = 0;
    jkGuiPatrons_numGold = 0;
    jkGuiPatrons_numOther = 0;
}

// Per-frame tick called from jkGuiRend_DisplayAndReturnClicked via menu->idkFunc.
// During the grace period (JKPATRONS_GRACE_MS), any user dismissal is reverted
// so the viewer has time to read the list. After the grace period, normal
// menu escape/click handling takes over.
static void jkGuiPatrons_TickFn(jkGuiMenu* menu)
{
    if (pHS->getTimerTick() - jkGuiPatrons_startMs < JKPATRONS_GRACE_MS)
    {
        menu->lastClicked = 0;
    }
}

// Build the element list for this session, auto-laying out gold names into
// however many columns are needed to fit the available vertical space.
static void jkGuiPatrons_BuildElements(void)
{
    const int screenW = 640;
    const int screenH = 480;
    const int titleH  = 40;
    const int goldH   = 22;
    const int gapH    = 14;
    const int footerH = 20;
    const int marginTop    = 12;
    const int marginBottom = 12;

    int footerBlockH = jkGuiPatrons_numOther
                     ? gapH + jkGuiPatrons_numOther * footerH
                     : 0;

    int goldAreaTop    = marginTop + titleH + gapH;
    int goldAreaBottom = screenH - marginBottom - footerBlockH;
    int goldAreaH      = goldAreaBottom - goldAreaTop;
    if (goldAreaH < goldH) goldAreaH = goldH;

    int rowsPerCol = goldAreaH / goldH;
    if (rowsPerCol < 1) rowsPerCol = 1;

    int numCols = (jkGuiPatrons_numGold + rowsPerCol - 1) / rowsPerCol;
    if (numCols < 1) numCols = 1;

    // Distribute names as evenly as possible across columns.
    int baseRows  = jkGuiPatrons_numGold / numCols;
    int extraRows = jkGuiPatrons_numGold % numCols;
    int tallestCol = baseRows + (extraRows ? 1 : 0);

    int actualGoldBlockH = tallestCol * goldH;
    int goldStartY = goldAreaTop + (goldAreaH - actualGoldBlockH) / 2 + goldH;
    if (goldStartY < goldAreaTop) goldStartY = goldAreaTop;

    int colW = screenW / numCols;

    int idx = 0;

    // Full-screen invisible dismiss button. Drawn first so text overlays it.
    // Empty string -> stdFont_Draw3 renders nothing, so no visual artefact,
    // but the element remains clickable (mouse / VR trigger) and serves as the
    // escape-key / return-key shortcut target so Esc dismisses as well.
    {
        jkGuiElement* e = &jkGuiPatrons_elements[idx++];
        e->type = ELEMENT_TEXTBUTTON;
        e->hoverId = -1;
        e->textType = JKPATRONS_FONT_FOOTER;
        e->origStr = NULL;
        e->rect.x = 0;
        e->rect.y = 0;
        e->rect.width = screenW;
        e->rect.height = screenH;
        e->bIsVisible = 1;
        e->enableHover = 0;
    }

    // Title (spans full width)
    {
        jkGuiElement* e = &jkGuiPatrons_elements[idx++];
        e->type = ELEMENT_TEXT;
        e->textType = JKPATRONS_FONT_TITLE;
        e->origStr = jkGuiPatrons_sTitle;
        e->rect.x = 0;
        e->rect.y = marginTop;
        e->rect.width = screenW;
        e->rect.height = titleH;
        e->bIsVisible = 1;
    }

    // Gold patrons, column-major fill (fill col 0 top-to-bottom, then col 1, ...).
    int nameIdx = 0;
    for (int col = 0; col < numCols && nameIdx < jkGuiPatrons_numGold; col++)
    {
        int rowsInThisCol = baseRows + (col < extraRows ? 1 : 0);
        for (int row = 0; row < rowsInThisCol && nameIdx < jkGuiPatrons_numGold; row++)
        {
            jkGuiElement* e = &jkGuiPatrons_elements[idx++];
            e->type = ELEMENT_TEXT;
            e->textType = JKPATRONS_FONT_GOLD;
            e->origStr = jkGuiPatrons_sGold[nameIdx++];
            e->rect.x = col * colW;
            e->rect.y = goldStartY + row * goldH;
            e->rect.width = colW;
            e->rect.height = goldH;
            e->bIsVisible = 1;
        }
    }

    // Footer
    if (jkGuiPatrons_numOther)
    {
        int y = screenH - marginBottom - jkGuiPatrons_numOther * footerH;
        for (int i = 0; i < jkGuiPatrons_numOther; i++)
        {
            jkGuiElement* e = &jkGuiPatrons_elements[idx++];
            e->type = ELEMENT_TEXT;
            e->textType = JKPATRONS_FONT_FOOTER;
            e->origStr = jkGuiPatrons_sOther[i];
            e->rect.x = 0;
            e->rect.y = y;
            e->rect.width = screenW;
            e->rect.height = footerH;
            e->bIsVisible = 1;
            y += footerH;
        }
    }

    jkGuiPatrons_elements[idx].type = ELEMENT_END;
}

void jkGuiPatrons_ShowAndWait(void)
{
    if (!jkGuiPatrons_bLoaded) return;
    if (jkGuiPatrons_numGold == 0 && jkGuiPatrons_numOther == 0) return;

    // Zero the element array so every field (including QOL unions) starts clean.
    for (int i = 0; i < JKPATRONS_MAX_ELEMENTS; i++)
    {
        _memset(&jkGuiPatrons_elements[i], 0, sizeof(jkGuiElement));
    }
    _memset(&jkGuiPatrons_menu, 0, sizeof(jkGuiMenu));

    jkGuiPatrons_BuildElements();

    jkGuiPatrons_menu.paElements = jkGuiPatrons_elements;
    jkGuiPatrons_menu.clickableIdxIdk = -1;
    jkGuiPatrons_menu.textBoxCursorColor = 0xFFFF;
    jkGuiPatrons_menu.fillColor = 0xF;
    jkGuiPatrons_menu.checkboxBitmapIdx = 0;
    jkGuiPatrons_menu.ui_structs = jkGui_stdBitmaps;
    jkGuiPatrons_menu.fonts = jkGui_stdFonts;
    jkGuiPatrons_menu.idkFunc = jkGuiPatrons_TickFn;

    // Ensure we're in menu display mode with the main menu palette.
    stdBitmap_EnsureData(jkGui_stdBitmaps[JKGUI_BM_BK_MAIN]);
    if (!jkGui_GdiMode)
    {
        jkGui_SetModeMenu(jkGui_stdBitmaps[JKGUI_BM_BK_MAIN]->palette);
    }

    jkGui_InitMenu(&jkGuiPatrons_menu, jkGui_stdBitmaps[JKGUI_BM_BK_MAIN]);

    // Escape and return keys both dismiss via the first element (the full-screen
    // invisible button). Mouse click / VR trigger on any part of the screen also
    // clicks this button directly.
    jkGuiRend_MenuSetEscapeKeyShortcutElement(&jkGuiPatrons_menu, &jkGuiPatrons_elements[0]);
    jkGuiRend_MenuSetReturnKeyShortcutElement(&jkGuiPatrons_menu, &jkGuiPatrons_elements[0]);

    jkGuiPatrons_startMs = pHS->getTimerTick();
    jkGuiRend_DisplayAndReturnClicked(&jkGuiPatrons_menu);
}

#endif // QOL_IMPROVEMENTS
