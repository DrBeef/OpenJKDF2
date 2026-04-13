#ifndef _JKGUIPATRONS_H
#define _JKGUIPATRONS_H

#include "types.h"

#ifdef QOL_IMPROVEMENTS

int  jkGuiPatrons_Startup(const char* fpath);
void jkGuiPatrons_Shutdown(void);
void jkGuiPatrons_ShowAndWait(void);

#endif

#endif
