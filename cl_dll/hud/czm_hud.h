/* Original CZM in-match HUD. GPL-2.0-or-later. */
#pragma once
#include "../../common/czm_hud_layout.h"
void CZM_HudVidInit();
bool CZM_HudDraw(int kind, const char *value, bool warning=false);
bool CZM_HudNumber(int kind, int value, bool warning=false);
