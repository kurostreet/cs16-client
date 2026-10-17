"""Execute real engine snapshots, menu HTTPS handoff and HUD image routing."""
from pathlib import Path
import re
import subprocess
import tempfile
import unittest
from test_loading_errors import function

ROOT = Path(__file__).resolve().parents[1]


def run_cpp(folder, source, extra=()):
    path = Path(folder)
    (path / 'main.cpp').write_text(source)
    exe = path / 'check'
    subprocess.run(['g++', '-std=c++11', '-I'+folder, '-I'+str(ROOT/'common'),
                    str(path/'main.cpp'), *map(str, extra), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)


VECTOR = r'''
#pragma once
#include <vector>
template<class T>class CUtlVector{std::vector<T> v;public:int Count()const{return v.size();}
int AddToTail(const T &t){v.push_back(t);return v.size()-1;}void RemoveAll(){v.clear();}
T &operator[](int n){return v[n];}const T &operator[](int n)const{return v[n];}};
'''
STRING = r'''
#pragma once
#include <string>
#include <cstdio>
class CUtlString{std::string s;public:void Set(const char *v){s=v;}const char *Get()const{return s.c_str();}
void SetDirect(const char *p,size_t n){s.assign(p,n);}void Append(const char *v){s+=v;}int Length()const{return s.size();}
template<typename... A>void AppendFormat(const char *f,A... a){char b[64];snprintf(b,sizeof(b),f,a...);s+=b;}};
'''


class TabAvatarTests(unittest.TestCase):
    def test_snapshot_uses_actual_server_userids_and_only_public_tags(self):
        source = (ROOT/'3rdparty/xash3d-fwgs/engine/client/cl_main.c').read_text()
        declaration = re.search(r'^static CVAR_DEFINE_AUTO\( czm_tab_state,.*$', source, re.M).group()
        cvars = (ROOT/'3rdparty/xash3d-fwgs/engine/common/cvar.c').read_text()
        harness = r'''
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include "czm_tab.h"
typedef bool qboolean;
enum{FCVAR_READ_ONLY=1,FCVAR_PRIVILEGED=2,FCVAR_CHEAT=4,FCVAR_LATCH=8,ca_active=3};
struct convar_t{const char *name;char *string;float value;uint32_t flags;};
#define CVAR_DEFINE_AUTO(n,s,f,d) convar_t n={#n,strdup(s),0,f}
#define Q_strlen strlen
#define Q_strcmp strcmp
#define Q_snprintf snprintf
#define Q_min std::min
#define FBitSet(f,b) ((f)&(b))
#define SetBits(f,b) ((f)|=(b))
#define Q_atof atof
#define Mem_Realloc(pool,ptr,size) ((char *)realloc(ptr,size))
static void Q_strncpy(char *a,const char *b,size_t n){strncpy(a,b,n);a[n-1]=0;}
static void Cvar_Changed(convar_t*){}
static int rejected;
static void Con_Printf(const char*,...){rejected++;}
static bool SV_Active(){return false;}static bool CL_Active(){return false;}
static struct{bool allow_cheats;}host;
struct player_info_t{char name[64],userinfo[128];int userid;};
static struct{int state;bool demoplayback;struct{int remote_address;}netchan;}cls;
static struct{int maxclients,playernum;player_info_t players[32];}cl;
static const char *NET_AdrToString(int){return "203.0.113.10:27015";}
static const char *Info_ValueForKey(const char *s,const char *key){assert(!strcmp(key,"czm_tag"));return s;}
''' + declaration + '\n' + function(cvars,'static qboolean Cvar_CanSet(') + '\n' + function(cvars,'void Cvar_DirectFullSet(') + '\n' + function(source,'static void CL_CzmTabState(') + r'''
int main(){
 assert(!Cvar_CanSet(&czm_tab_state)&&rejected==1);
 cls.state=ca_active;cl.maxclients=32;cl.playernum=0;
 for(int i=0;i<32;i++){strcpy(cl.players[i].name,"Same nickname");cl.players[i].userid=100+i;memset(cl.players[i].userinfo,'a'+i%6,32);cl.players[i].userinfo[32]=0;}
 CL_CzmTabState();CzmTab::State state;assert(CzmTab::Parse(czm_tab_state.string,state));assert(state.self==100&&state.players[32].userid==131);
 assert(!strcmp(state.server,"203.0.113.10:27015")&&!strstr(czm_tab_state.string,"Same nickname"));
 strcpy(cl.players[1].userinfo,"czms1_private-session-secret");CL_CzmTabState();assert(CzmTab::Parse(czm_tab_state.string,state)&&!state.players[2].tag[0]);
 assert(!strstr(czm_tab_state.string,"private-session"));cl.players[1].name[0]=0;CL_CzmTabState();assert(CzmTab::Parse(czm_tab_state.string,state)&&!state.players[2].userid);
 cl.players[1].userid=999;strcpy(cl.players[1].name,"New player");CL_CzmTabState();assert(CzmTab::Parse(czm_tab_state.string,state)&&state.players[2].userid==999);
 cls.demoplayback=true;CL_CzmTabState();assert(!*czm_tab_state.string);cls.demoplayback=false;cls.state=0;CL_CzmTabState();assert(!*czm_tab_state.string);
 assert(!CzmTab::Parse("203.0.113.10:27015 100\n33 101 aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n",state));
 assert(!CzmTab::Parse("203.0.113.10:27015 100\n2 101 aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n2 102 bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\n",state));
}
'''
        with tempfile.TemporaryDirectory() as folder: run_cpp(folder, harness)

    def test_async_photo_arrival_slot_reuse_server_account_switch_and_expiry(self):
        harness = r'''
#include <map>
#include <string>
#include <vector>
#include <cassert>
#include <cstring>
#include "utlstring.h"
static std::map<std::string,std::string> vars;
static double clockNow=100;static bool online=true,photoReady;static int player=7,nextId=1;
struct Request{int id;std::string url,body;};static std::vector<Request> requests;
static std::map<int,std::pair<int,std::string>> replies;
struct EngFuncs{static const char *GetCvarString(const char *k){return vars[k].c_str();}
static void CvarSetString(const char *k,const char *v){vars[k]=v;}static double DoubleTime(){return clockNow;}};
static bool CZM_OnlineReady(){return online;}static int CZM_OnlinePlayer(){return player;}
static const char *CZM_OnlineKey(){return "private-owner-key";}static const char *CZM_OnlineUrl(const char *p){return p;}
static const char *CZM_AvatarFor(const char *ref,int p){assert(p==8);return !strcmp(ref,"photo:1")&&photoReady?"czm_cache/av_8_1.png":"gfx/czm/ui/avatar.png";}
static int CZM_NetRequest(const char *method,const char *url,const char *key,const char *body){assert(!strcmp(method,"POST")&&!strcmp(key,"private-owner-key"));assert(!strstr(body,key));requests.push_back({nextId,url,body});return nextId++;}
static bool CZM_NetPoll(int id,int *status,CUtlString &body){auto it=replies.find(id);if(it==replies.end())return false;*status=it->second.first;body.Set(it->second.second.c_str());replies.erase(it);return true;}
static void Q_strncpy(char *a,const char *b,size_t n){strncpy(a,b,n);a[n-1]=0;}
#define Q_min(a,b) ((a)<(b)?(a):(b))
void CZM_TabReset();void CZM_TabFrame();
''' + (ROOT/'czm/menu/CzmTab.cpp').read_text().replace('#include "../../common/czm_tab.h"', '#include "czm_tab.h"') + r'''
static void Snapshot(const char *server="203.0.113.10:27015",int userid=42){vars["czm_tab_state"]=std::string(server)+" 41\n1 41 -\n2 "+std::to_string(userid)+" aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n";}
static std::string Photo(int userid=42){return "{\"players\":[{\"userid\":"+std::to_string(userid)+",\"tag\":\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\",\"id\":8,\"avatar\":\"photo:1\",\"ttl\":180}]}";}
int main(){
 Snapshot();CZM_TabFrame();assert(requests.size()==2);int bind=requests[0].id,resolve=requests[1].id;
 replies[bind]={200,"{\"tag\":\"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\",\"ttl\":180}"};replies[resolve]={200,Photo()};CZM_TabFrame();
 assert(vars["czm_tag"]=="bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");assert(vars["czm_tab_avatar_2"].find("gfx/czm/ui/avatar.png")!=std::string::npos);
 photoReady=true;CZM_TabFrame();assert(vars["czm_tab_avatar_2"]=="203.0.113.10:27015|42|aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa|czm_cache/av_8_1.png");assert(requests.size()==2);
 clockNow=116;CZM_TabFrame();int oldResolve=requests.back().id;
 Snapshot("203.0.113.10:27015",43);replies[oldResolve]={200,Photo()};CZM_TabFrame();assert(vars["czm_tab_avatar_2"].empty());
 int slotResolve=requests.back().id;assert(slotResolve!=oldResolve);Snapshot("203.0.113.11:27015");CZM_TabFrame();assert(vars["czm_tag"].empty());
 replies[slotResolve]={200,Photo(43)};CZM_TabFrame();assert(vars["czm_tab_avatar_2"].empty());
 int accountBind=0;for(auto &r:requests)if(r.url.find("/bind")!=std::string::npos)accountBind=r.id;
 CZM_TabReset();player=9;replies[accountBind]={200,"{\"tag\":\"cccccccccccccccccccccccccccccccc\",\"ttl\":180}"};CZM_TabFrame();assert(vars["czm_tag"].empty());
 int freshBind=requests.back().id;for(auto &r:requests)if(r.url.find("/bind")!=std::string::npos)freshBind=r.id;
 replies[freshBind]={200,"{\"tag\":\"dddddddddddddddddddddddddddddddd\",\"ttl\":180}"};
 int freshResolve=requests.back().id;if(requests.back().url.find("/resolve")!=std::string::npos)replies[freshResolve]={200,Photo()};
 CZM_TabFrame();assert(vars["czm_tag"]=="dddddddddddddddddddddddddddddddd");
 clockNow=400;CZM_TabFrame();assert(vars["czm_tag"].empty()&&vars["czm_tab_avatar_2"].empty());
 vars["czm_tab_state"]="";CZM_TabFrame();assert(vars["czm_tag"].empty()&&vars["czm_tab_avatar_2"].empty());
}
'''
        with tempfile.TemporaryDirectory() as folder:
            p = Path(folder)
            (p/'utlvector.h').write_text(VECTOR); (p/'utlstring.h').write_text(STRING)
            for name in ['extdll_menu.h','BaseMenu.h','CzmMenu.h']: (p/name).write_text('')
            (p/'CzmJson.h').write_text((ROOT/'czm/menu/CzmJson.h').read_text())
            run_cpp(folder,harness,[ROOT/'czm/menu/CzmJson.cpp'])

    def test_hud_draws_own_and_verified_avatars_but_rejects_stale_or_arbitrary_paths(self):
        stub = r'''
#pragma once
#include <cstddef>
typedef unsigned char byte;
enum{TF_NOMIPMAP=1,TF_CLAMP=2,kRenderTransAlpha=1};
struct Board { bool m_bForceDraw=false,m_bShowscoresHeld=false; bool ShouldDrawScoreboard()const{return m_bForceDraw||m_bShowscoresHeld;} };
struct Hud{float m_flScale;Board m_Scoreboard;};
extern int ScreenWidth,ScreenHeight;extern Hud gHUD;extern int g_iXash;
struct Tri{void(*RenderMode)(int);void(*Color4f)(float,float,float,float);};
struct Engine{const char *(*pfnGetCvarString)(const char*);double(*GetClientTime)();byte*(*COM_LoadFile)(const char*,int,int*);void(*COM_FreeFile)(void*);Tri*pTriAPI;float(*pfnGetCvarFloat)(const char*);const char*(*pfnGetLevelName)();void(*Cvar_Set)(const char*,const char*);void(*pfnClientCmd)(const char*);};extern Engine gEngfuncs;
struct Render{int(*GL_LoadTexture)(const char*,const byte*,size_t,int);void(*GL_FreeTexture)(unsigned);void(*GL_SelectTexture)(int);void(*GL_Bind)(int,int);};extern Render gRenderAPI;
class DrawUtils{public:static void Draw2DQuad(float,float,float,float);};
'''
        harness = r'''
#include "hud.h"
#include "czm_tab.h"
#include <map>
#include <string>
#include <vector>
#include <cassert>
#include <cstring>
#include <cstdlib>
Hud gHUD={2};int g_iXash=1;int ScreenWidth=1600,ScreenHeight=900;static int commands;static double testTime=100;static std::map<std::string,std::string> vars;static std::vector<std::string> loaded;static int freed,quads;
const char *Get(const char *k){return vars[k].c_str();}double Clock(){return testTime;}
float Float(const char *k){return atof(Get(k));}const char *Level(){return "maps/zm_test.bsp";}void Set(const char*k,const char*v){vars[k]=v;}void Command(const char*k){assert(!strcmp(k,"menu_czmsocial\n"));commands++;}
byte *File(const char *p,int,int *n){assert(CzmTab::AvatarPath(p));loaded.push_back(p);*n=4;return (byte*)malloc(4);}
void Free(void*p){free(p);}void Mode(int){}void Color(float,float,float,float){}
Tri tri={Mode,Color};Engine gEngfuncs={Get,Clock,File,Free,&tri,Float,Level,Set,Command};
int Load(const char *p,const byte *,size_t,int){assert(!strncmp(p,"czm_tab/",8));return loaded.size();}
void Release(unsigned){freed++;}void Select(int){}void Bind(int,int){}Render gRenderAPI={Load,Release,Select,Bind};
void DrawUtils::Draw2DQuad(float x,float y,float x2,float y2){assert(x==20&&y==40&&x2==68&&y2==88);quads++;}
''' + (ROOT/'cl_dll/hud/czm_avatars.cpp').read_text() + r'''
int main(){
 vars["czm_avatar"]="3";assert(CZM_DrawTabAvatar(1,true,10,20,24));assert(loaded.back()=="gfx/czm/avatars/avatar_03.png");
 vars["czm_avatar"]="photo";vars["czm_avatar_photo"]="czm_avatar_123456.png";CZM_DrawTabAvatar(1,true,10,20,24);assert(loaded.back()=="czm_avatar_123456.png");
 vars["czm_tab_state"]="203.0.113.10:27015 41\n1 41 -\n2 42 aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n";
 vars["czm_tab_avatar_2"]="203.0.113.10:27015|42|aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa|czm_cache/av_8_1.png";
 CZM_DrawTabAvatar(2,false,10,20,24);assert(loaded.back()=="czm_cache/av_8_1.png");
 vars["czm_tab_avatar_2"]="203.0.113.10:27015|42|aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa|../../private.png";
 CZM_DrawTabAvatar(2,false,10,20,24);assert(loaded.back()=="gfx/czm/ui/icon_user.png");
 vars["czm_tab_avatar_2"]="203.0.113.11:27015|42|aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa|czm_cache/av_99_1.png";size_t count=loaded.size();
 CZM_DrawTabAvatar(2,false,10,20,24);assert(loaded.size()==count);
 vars["czm_tab_avatar_2"]="203.0.113.10:27015|43|aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa|czm_cache/av_99_1.png";
 CZM_DrawTabAvatar(2,false,10,20,24);assert(loaded.size()==count);
 assert(!CzmTab::AvatarPath("gfx/czm/avatars/avatar_01.png/evil"));assert(!CzmTab::AvatarPath("czm_cache/av_../evil.png"));

 assert(quads==6);CZM_TabAvatarsVidInit();assert(freed==4);
 CZM_TabConnectionFrame();assert(vars["czm_current_map"]=="zm_test");
 vars["czm_tab_profile_2"]="203.0.113.10:27015|42|aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa|8";
 vars["czm_touch_profile_aspect"]="0.5625";
 gHUD.m_Scoreboard.m_bForceDraw=true;CZM_TabProfilesBeginDraw();CZM_TabProfileRow(2,false,100,100,300,40);
 float x=150.0f/ScreenWidth,y=110.0f/(ScreenWidth*.5625f);
 assert(CZM_TabProfileTouch(0,7,x,y)==1);assert(CZM_TabProfileTouch(1,7,x,y)==1);
 assert(commands==1&&vars["czm_open_profile"]=="8"&&!gHUD.m_Scoreboard.m_bForceDraw);
 // A recycled game slot must not open the previous player's account.
 gHUD.m_Scoreboard.m_bForceDraw=true;CZM_TabProfilesBeginDraw();CZM_TabProfileRow(2,false,100,100,300,40);
 assert(CZM_TabProfileTouch(0,7,x,y)==1);
 vars["czm_tab_state"]="203.0.113.10:27015 41\n1 41 -\n2 43 aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n";
 assert(CZM_TabProfileTouch(1,7,x,y)==1&&commands==1);
 vars["czm_tab_state"]="203.0.113.10:27015 41\n1 41 -\n2 42 aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n";
 assert(CZM_TabProfileTouch(0,7,x,y)==1);CZM_TabProfileTouch(2,7,x+.2f,y);CZM_TabProfileTouch(1,7,x+.2f,y);assert(commands==1);
 // Profile aspect is independent from the current screen aspect.
 ScreenWidth=2000;ScreenHeight=900;x=150.0f/ScreenWidth;y=110.0f/(ScreenWidth*.5625f);
 CZM_TabProfilesBeginDraw();CZM_TabProfileRow(2,false,100,100,300,40);
 assert(CZM_TabProfileTouch(0,7,x,y)==1);assert(CZM_TabProfileTouch(1,7,x,y)==1&&commands==2);
 gHUD.m_Scoreboard.m_bForceDraw=true;testTime+=2;
 assert(CZM_TabProfileTouch(0,7,x,y)==0);
 vars["czm_tab_profile_2"]="";CZM_TabProfilesBeginDraw();CZM_TabProfileRow(2,false,100,100,300,40);
 assert(CZM_TabProfileTouch(0,7,x,y)==0);

}
'''
        with tempfile.TemporaryDirectory() as folder:
            p=Path(folder);(p/'hud.h').write_text(stub)
            for name in ['cl_util.h','draw_util.h','triangleapi.h']:(p/name).write_text('')
            (p/'czm_avatars.h').write_text((ROOT/'cl_dll/hud/czm_avatars.h').read_text())
            run_cpp(folder,harness)


if __name__ == '__main__': unittest.main()
