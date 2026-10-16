/* Shared HUD geometry, independent of the renderer. GPL-2.0-or-later. */
#pragma once
#include <cmath>
#include <cstdio>

namespace CzmHud {
enum { HEALTH, ARMOR, TIMER, MONEY, AMMO, COUNT };
static const char *const keys[COUNT] = { "hp", "armor", "time", "money", "ammo" };
static const float defaults[COUNT][2] = {{.085f,.94f},{.25f,.94f},{.5f,.94f},{.88f,.82f},{.86f,.94f}};
inline float Clamp(float v, float low, float high, float fallback) {
    return std::isfinite(v) ? (v < low ? low : v > high ? high : v) : fallback;
}
inline void Key(char *out, size_t n, int kind, const char *field) {
    snprintf(out, n, "czm_hud_%s_%s", keys[kind], field);
}
struct Rect { float x,y,w,h,scale,opacity; };
inline Rect Layout(int kind, int style, float sw, float sh, float x, float y, float scale, float opacity, int digits) {
    scale = Clamp(scale,.65f,1.6f,1);
    opacity = Clamp(opacity,.25f,1,1);
    const float unit=sh/480.0f * scale;
    float h=(style==1?32:style==2?52:44)*unit;
    float w=(34 + (digits<5?5:digits)*17)*unit;
    if(w>sw*.38f) w=sw*.38f;
    if(h>sh*.17f) h=sh*.17f;
    x=Clamp(x,.01f,.99f,defaults[kind][0])*sw-w/2;
    y=Clamp(y,.01f,.99f,defaults[kind][1])*sh-h/2;
    x=Clamp(x,sw*.012f,sw-sw*.012f-w,sw*.012f);
    y=Clamp(y,sh*.018f,sh-sh*.018f-h,sh*.018f);
    return {x,y,w,h,unit,opacity};
}
}
