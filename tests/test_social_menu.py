"""Run the native social window against delayed HTTPS replies and account changes."""
from pathlib import Path
import os
import subprocess
import tempfile
import unittest
from test_tab_avatars import VECTOR, STRING

ROOT = Path(__file__).resolve().parents[1]
STUB = r'''
#pragma once
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include "utlvector.h"
#include "utlstring.h"
using HFont=int;
enum{CZMF_PANEL_TEXT,CZMF_LANG,CZMF_SMALL,CZMF_PANEL_TITLE,QM_CENTER=1,QM_RIGHT=2,CZM_TAB_PROFILE=5,K_MWHEELUP=30,K_MWHEELDOWN=31};
template<class T> T Q_min(T a,T b){return std::min(a,b);}template<class T>T Q_max(T a,T b){return std::max(a,b);}
template<class T>T bound(T a,T b,T c){return std::max(a,std::min(b,c));}
inline void Q_strncpy(char*a,const char*b,size_t n){if(a!=b){strncpy(a,b,n);a[n-1]=0;}}
inline unsigned PackRGBA(int r,int g,int b,int a){return (unsigned)r|((unsigned)g<<8)|((unsigned)b<<16)|((unsigned)a<<24);}
extern int ScreenWidth,ScreenHeight,cursorX,cursorY;
struct CzmRect {int x,y,w,h;bool HasCursor()const{return cursorX>=x&&cursorX<x+w&&cursorY>=y&&cursorY<y+h;}};
inline CzmRect CZM_Rect(int x,int y,int w,int h){return{x,y,w,h};}
struct Stack{void*current=nullptr;void*Current(){return current;}};extern Stack stack;
class CMenuFramework{public:CMenuFramework(const char*){m_pStack=&stack;}virtual~CMenuFramework(){}virtual void Show(){stack.current=this;}virtual void Hide(){if(stack.current==this)stack.current=nullptr;}
virtual void Draw(){}virtual bool IsRoot()const{return false;}virtual bool KeyDown(int){return false;}virtual bool KeyUp(int){return false;}virtual bool MouseMove(int,int){return false;}virtual void Char(int){}protected:Stack*m_pStack;};
#define ADD_MENU3(cmd,type,fn) static type*cmd=new type();void fn()
namespace UI{namespace Key{inline bool IsBackspace(int k){return k==8;}inline bool IsEnter(int k){return k==13;}inline bool IsEscape(int k){return k==27;}inline bool IsLeftMouse(int k){return k==1;}}
namespace Scissor{inline void PushScissor(int,int,int,int){}inline void PopScissor(){}}}
struct Cursor{int cursorY;} ;extern Cursor uiStatic;
struct EngFuncs{static const char*GetCvarString(const char*);static float GetCvarFloat(const char*);static void CvarSetString(const char*,const char*);static void CvarSetValue(const char*,float);static double DoubleTime();static void SetTextInputRect(int,int,int,int){}};
inline bool CL_IsActive(){return false;}
void UI_EnableTextInput(bool);void CZM_Init();bool CZM_IsRussian();unsigned CZM_White();unsigned CZM_Grey();
HFont CZM_Font(int);int CZM_FontHeight(HFont);int CZM_TextWidth(HFont,const char*);int CZM_WrapText(HFont,const char*,int,int*,int);void CZM_WrapLine(const char*,const int*,int,char*,size_t);
void CZM_DrawText(HFont,int,int,int,const char*,unsigned,unsigned=0);void UI_FillRect(int,int,int,int,unsigned);
void CZM_DrawImage(const char*,int,int,int,int,unsigned=0xffffffff);void CZM_DrawPill(const char*,int,int,int,int,unsigned=0xffffffff);void CZM_DrawButton(const char*,const CzmRect&,const char*,unsigned,bool,int);void CZM_DrawPanel(const CzmRect&);
bool CZM_OnlineReady();const char*CZM_OnlineKey();int CZM_OnlinePlayer();const char*CZM_OnlineUrl(const char*);const char*CZM_AvatarFor(const char*,int);
int CZM_NetRequest(const char*,const char*,const char*,const char*);bool CZM_NetPoll(int,int*,CUtlString&);void CZM_NetNewKey(char*,size_t);
void CZM_OpenSettings(int);void CZM_AccountShow(const char*,const char*);bool CZM_CopyText(const char*);void CZM_JoinAddress(const char*);void CZM_PlayClick();
'''
HARNESS = r'''
#include "stub.h"
#include <map>
#include <cassert>
int ScreenWidth=1536,ScreenHeight=716,cursorX,cursorY;Cursor uiStatic;Stack stack;
static double now=100;static std::string key="czms1_first-account";static int me=7,ids,logins,lastButtonFont;static bool drawTrace;
static std::map<std::string,std::string> vars;static std::map<int,std::pair<int,std::string>> replies;
struct Request{int id;std::string url,body;};static std::vector<Request> requests;
const char*EngFuncs::GetCvarString(const char*k){return vars[k].c_str();}float EngFuncs::GetCvarFloat(const char*k){return atof(GetCvarString(k));}void EngFuncs::CvarSetString(const char*k,const char*v){vars[k]=v;}void EngFuncs::CvarSetValue(const char*k,float v){vars[k]=std::to_string(v);}double EngFuncs::DoubleTime(){return now;}
void UI_EnableTextInput(bool){}void CZM_Init(){}bool CZM_IsRussian(){return true;}unsigned CZM_White(){return 0xffffffff;}unsigned CZM_Grey(){return PackRGBA(150,164,184,255);}
HFont CZM_Font(int i){return i;}int CZM_FontHeight(HFont f){return (f==CZMF_PANEL_TITLE?42:f==CZMF_LANG?24:f==CZMF_SMALL?17:21)*ScreenHeight/768;}
int CZM_TextWidth(HFont f,const char*s){int n=0;for(;*s;s++)if((*s&0xc0)!=0x80)n++;return n*CZM_FontHeight(f)*62/100;}
int CZM_WrapText(HFont f,const char*s,int w,int*starts,int max){int lines=0,i=0,last=0;starts[0]=0;for(;s[i]&&lines<max;){int from=i,n=0;while(s[i]&&(n*CZM_FontHeight(f)/2<w||!n)){i++;while((s[i]&0xc0)==0x80)i++;n++;}if(i==from)break;lines++;starts[lines]=i;last=i;}return lines;}
void CZM_WrapLine(const char*s,const int*starts,int n,char*out,size_t size){int len=std::min((int)size-1,starts[n+1]-starts[n]);memcpy(out,s+starts[n],len);out[len]=0;}
void CZM_DrawText(HFont f,int x,int y,int w,const char*s,unsigned c,unsigned just){if(drawTrace){printf("T\t%d\t%d\t%d\t%d\t%u\t%u\t%s\n",x,y,w,CZM_FontHeight(f),c,just,s);}}
void UI_FillRect(int x,int y,int w,int h,unsigned c){if(drawTrace)printf("R\t%d\t%d\t%d\t%d\t%u\n",x,y,w,h,c);}
void CZM_DrawImage(const char*p,int x,int y,int w,int h,unsigned c){if(drawTrace)printf("I\t%d\t%d\t%d\t%d\t%s\n",x,y,w,h,p);}
void CZM_DrawPill(const char*,int x,int y,int w,int h,unsigned c){UI_FillRect(x,y,w,h,c);}
void CZM_DrawPanel(const CzmRect&r){UI_FillRect(r.x,r.y,r.w,r.h,PackRGBA(10,17,26,250));}
void CZM_DrawButton(const char*p,const CzmRect&r,const char*s,unsigned c,bool,int font){lastButtonFont=font;UI_FillRect(r.x,r.y,r.w,r.h,strstr(p,"primary")?PackRGBA(210,34,55,255):PackRGBA(31,43,59,255));CZM_DrawText(font,r.x+r.h/4,r.y+(r.h-CZM_FontHeight(font))/2,r.w-r.h/2,s,c);}
bool CZM_OnlineReady(){return true;}const char*CZM_OnlineKey(){return key.c_str();}int CZM_OnlinePlayer(){return me;}const char*CZM_OnlineUrl(const char*p){return p;}const char*CZM_AvatarFor(const char*,int){return "gfx/czm/ui/avatar.png";}
int CZM_NetRequest(const char*,const char*url,const char*,const char*body){requests.push_back({++ids,url,body});return ids;}
bool CZM_NetPoll(int id,int*status,CUtlString&body){auto i=replies.find(id);if(i==replies.end())return false;*status=i->second.first;body.Set(i->second.second.c_str());replies.erase(i);return true;}
void CZM_NetNewKey(char*out,size_t size){Q_strncpy(out,"0123456789abcdef0123456789abcdef",size);}void CZM_OpenSettings(int){}void CZM_AccountShow(const char*,const char*){logins++;}bool CZM_CopyText(const char*){return true;}void CZM_JoinAddress(const char*){}void CZM_PlayClick(){}
'''
SCENARIOS = r'''
int main(int argc,char**argv){
 CZM_SocialFrame();CZM_OpenFriends();auto&w=*menu_czmsocial;w.Frame();assert(w.m_read);
 int old=w.m_read;w.m_tab=1;w.Navigate(w.LIST);replies[old]={200,"{\"people\":[{\"id\":99,\"nick\":\"stale\"}]}"};w.Frame();assert(w.m_people.Count()==0);
 CzmJson j;assert(j.Parse("{\"profile\":{\"id\":8,\"nick\":\"Hunter\",\"account\":true,\"relation\":\"friend\",\"online\":true,\"status\":\"Выживаем вместе\",\"created\":1792240000,\"counts\":{\"friends\":234,\"followers\":47}},\"can_send\":true,\"messages\":[{\"id\":1,\"p\":8,\"t\":\"Привет! Заходи играть\",\"ts\":1792240000}]}"));
 w.m_target=8;w.Navigate(w.DIALOG);w.Read(j,w.READ_DIALOG);assert(w.m_canSend&&w.m_messages.Count()==1);
 strcpy(w.m_input,"Hello friend");w.Send();int send=w.m_write;auto payload=requests.back().body;replies[send]={0,""};w.Frame();assert(!w.m_write&&!strcmp(w.m_input,"Hello friend"));
 w.Send();assert(requests.back().body==payload);replies[w.m_write]={200,"{\"message\":{\"id\":2}}"};w.Frame();assert(!*w.m_input);
 // A read follower list must not start a mark-seen/refetch request loop.
 w.m_tab=2;w.Navigate(w.LIST);CzmJson followers;assert(followers.Parse("{\"people\":[],\"counts\":{\"requests\":0},\"next\":-1}"));
 w.Read(followers,w.READ_LIST);assert(w.m_write==0);
 // A reply sent before logout cannot populate another account's window.
 int pending=w.m_read;key="czms1_second-account";me=9;CZM_SocialFrame();assert(!w.m_open&&w.m_messages.Count()==0);
 if(pending)replies[pending]={200,"{\"messages\":[{\"id\":55,\"t\":\"private old account\"}]}"};CZM_SocialFrame();assert(w.m_messages.Count()==0);
 key="guest-legacy-key";CZM_SocialFrame();CZM_OpenFriends();w.Layout();cursorX=w.m_prev.x+1;cursorY=w.m_prev.y+1;assert(w.Hit()==w.LOGIN);w.Activate(w.LOGIN);assert(logins==1);
 Button(CZM_Rect(0,0,ScreenHeight*.18f,ScreenHeight*.07f),"Отправить");assert(lastButtonFont!=CZMF_LANG);
 // The exact Draw methods also run at phone and narrow landscape sizes.
 key="czms1_second-account";CZM_SocialFrame();CZM_OpenFriends();
 for(auto resolution:{std::pair<int,int>{1536,716},{960,540},{1024,768}}){ScreenWidth=resolution.first;ScreenHeight=resolution.second;w.OpenProfile(8,false);w.Read(j,w.READ_PROFILE);w.Draw();assert(w.m_primary.y+w.m_primary.h<ScreenHeight);assert(w.m_copy.x+w.m_copy.w<w.m_panel.x+w.m_panel.w);}
 ScreenWidth=1536;ScreenHeight=716;
 if(argc>1){drawTrace=true;if(!strcmp(argv[1],"profile")){w.OpenProfile(8,false);w.Read(j,w.READ_PROFILE);}else{w.m_target=8;w.Navigate(w.DIALOG);w.Read(j,w.READ_DIALOG);}w.Draw();}
}
'''


class SocialMenuTests(unittest.TestCase):
    def test_delayed_replies_retry_and_account_isolation(self):
        with tempfile.TemporaryDirectory() as folder:
            p = Path(folder)
            vector = VECTOR.replace("void RemoveAll(){v.clear();}", "void RemoveAll(){v.clear();} void Remove(int n){v.erase(v.begin()+n);} T&Tail(){return v.back();}")
            (p / "utlvector.h").write_text(vector)
            (p / "utlstring.h").write_text(STRING.replace("void Set(const char *v)", "void Clear(){s.clear();}void Set(const char *v)"))
            (p / "stub.h").write_text(STUB)
            for name in ["extdll_menu.h", "BaseMenu.h", "Utils.h", "Framework.h", "Scissor.h", "keydefs.h", "CzmMenu.h"]:
                (p / name).write_text('#include "stub.h"\n')
            (p / "CzmJson.h").write_text((ROOT / "czm/menu/CzmJson.h").read_text())
            source = (ROOT / "czm/menu/CzmSocial.cpp").read_text().replace('#include "../../common/czm_tab.h"', '#include "czm_tab.h"').replace("private:\n\tenum", "public:\n\tenum")
            (p / "main.cpp").write_text(HARNESS + source + SCENARIOS)
            exe = p / "social"
            subprocess.run(["g++", "-std=c++11", "-w", "-I"+folder, "-I"+str(ROOT/"common"), str(p/"main.cpp"), str(ROOT/"czm/menu/CzmJson.cpp"), "-o", str(exe)], check=True)
            subprocess.run([str(exe)], check=True)
            preview = os.environ.get("CZM_SOCIAL_PREVIEW_DIR")
            if preview:
                output = Path(preview); output.mkdir(parents=True, exist_ok=True)
                for view in ["profile", "dialog"]:
                    result = subprocess.run([str(exe), view], check=True, capture_output=True, text=True)
                    (output/(view+".tsv")).write_text(result.stdout)


if __name__ == "__main__": unittest.main()
