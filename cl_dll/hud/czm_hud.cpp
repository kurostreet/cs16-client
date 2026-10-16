/* Original Inter-based CZM HUD; no legacy HUD sprites. GPL-2.0-or-later. */
#include "hud.h"
#include "cl_util.h"
#include "draw_util.h"
#include "czm_hud.h"
#include "triangleapi.h"
#include <cstring>
#include <algorithm>

static int s_atlas;
void CZM_HudVidInit() {
    s_atlas=gRenderAPI.GL_LoadTexture("gfx/czm/hud_glyphs.tga",NULL,0,TF_NOMIPMAP|TF_CLAMP);
}
static float Value(int kind,const char *field,float fallback) {
    char key[64]; CzmHud::Key(key,sizeof(key),kind,field);
    const char *s=gEngfuncs.pfnGetCvarString(key);
    return s && *s ? gEngfuncs.pfnGetCvarFloat(key) : fallback;
}
static void Glyphs(float x,float y,float height,const char *s,int r,int g,int b,float alpha) {
    gRenderAPI.GL_SelectTexture(0); gRenderAPI.GL_Bind(0,s_atlas);
    gEngfuncs.pTriAPI->RenderMode(kRenderTransAlpha);
    gEngfuncs.pTriAPI->Color4f(r/255.f,g/255.f,b/255.f,alpha);
    const float scale=gHUD.m_flScale, width=height*.65f;
    for(;*s;s++,x+=width) {
        unsigned c=(unsigned char)*s; if(c<32 || c>95) c='?'; c-=32;
        float u=(c%16)/16.f,v=(c/16)/4.f;
        gEngfuncs.pTriAPI->Begin(TRI_QUADS);
        gEngfuncs.pTriAPI->TexCoord2f(u,v);gEngfuncs.pTriAPI->Vertex3f(x*scale,y*scale,0);
        gEngfuncs.pTriAPI->TexCoord2f(u,v+.25f);gEngfuncs.pTriAPI->Vertex3f(x*scale,(y+height)*scale,0);
        gEngfuncs.pTriAPI->TexCoord2f(u+.0625f,v+.25f);gEngfuncs.pTriAPI->Vertex3f((x+width)*scale,(y+height)*scale,0);
        gEngfuncs.pTriAPI->TexCoord2f(u+.0625f,v);gEngfuncs.pTriAPI->Vertex3f((x+width)*scale,y*scale,0);
        gEngfuncs.pTriAPI->End();
    }
    gEngfuncs.pTriAPI->Color4f(1,1,1,1);
}
bool CZM_HudDraw(int kind,const char *value,bool warning) {
    if(!s_atlas || !g_iXash || kind<0 || kind>=CzmHud::COUNT) return false;
    int style=(int)CzmHud::Clamp(gEngfuncs.pfnGetCvarFloat("czm_hud_style"),0,2,0);
    CzmHud::Rect p=CzmHud::Layout(kind,style,ScreenWidth,ScreenHeight,Value(kind,"x",CzmHud::defaults[kind][0]),
        Value(kind,"y",CzmHud::defaults[kind][1]),Value(kind,"scale",1),Value(kind,"opacity",1),(int)strlen(value));
    int x=p.x,y=p.y,w=p.w,h=p.h,a=255*p.opacity;
    int red=warning?255:224,green=warning?65:37,blue=warning?72:50;
    int edge=h/18+1;
    if(style!=1) {
        FillRGBABlend(x,y,w,h,10,14,22,(style==2?220:165)*p.opacity);
        if(style==2) {
            FillRGBA(x,y,w,edge,red,green,blue,a);
            FillRGBA(x,y+h-edge,w,edge,62,70,88,a/2);
        } else FillRGBA(x,y,edge,h,red,green,blue,a);
    } else FillRGBA(x,y+h-edge,w,edge,warning?255:145,warning?65:160,warning?72:185,a/2);
    // Independently drawn geometric indicators: cross, shield, clock, wallet, cartridges.
    const int sz=h*.36f,ix=x+h*.18f,iy=y+(h-sz)/2,t=sz/5+1;
    const int ir=warning?255:214,ig=warning?70:224,ib=warning?78:239;
    if(kind==CzmHud::HEALTH) {
        FillRGBA(ix+sz/2-t/2,iy,t,sz,ir,ig,ib,a);FillRGBA(ix,iy+sz/2-t/2,sz,t,ir,ig,ib,a);
    } else if(kind==CzmHud::ARMOR) {
        for(int j=0;j<sz;j++) { int k=j>sz/2?(j-sz/2)/2:0;FillRGBA(ix+k,iy+j,sz-k*2,1,ir,ig,ib,a); }
    } else if(kind==CzmHud::TIMER) {
        FillRGBA(ix,iy,sz,t,ir,ig,ib,a);FillRGBA(ix,iy+sz-t,sz,t,ir,ig,ib,a);
        FillRGBA(ix,iy,t,sz,ir,ig,ib,a);FillRGBA(ix+sz-t,iy,t,sz,ir,ig,ib,a);
        FillRGBA(ix+sz/2,iy+t,t,sz/2,red,green,blue,a);
    } else if(kind==CzmHud::MONEY) {
        FillRGBA(ix,iy,sz,sz,ir,ig,ib,a);FillRGBA(ix+sz/2,iy+sz/3,sz/2,sz/3,red,green,blue,a);
    } else {
        for(int j=0;j<3;j++) FillRGBA(ix+j*(sz/3),iy+(j%2)*t,t,sz-(j%2)*t,ir,ig,ib,a);
    }
    float textH=h*(style==1?.7f:.52f),tx=ix+sz+h*.15f;
    float maxH=(x+w-tx-h*.12f)/(std::max(1,(int)strlen(value))*.65f);
    if(textH>maxH) textH=maxH;
    float ty=y+(h-textH)/2;
    if(style==2) {
        static const char *labels[]={"HEALTH","ARMOR","ROUND","CREDITS","AMMO"};
        Glyphs(tx,y+h*.13f,h*.16f,labels[kind],147,159,180,p.opacity);
        ty=y+h*.38f;
    }
    Glyphs(tx,ty,textH,value,warning?255:246,warning?100:249,warning?105:255,p.opacity);
    return true;
}
bool CZM_HudNumber(int kind,int value,bool warning) {
    char s[32]; snprintf(s,sizeof(s),"%d",value);return CZM_HudDraw(kind,s,warning);
}
