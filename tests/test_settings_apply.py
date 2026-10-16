"""Execute the native settings handoff, including a failed second layout write."""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class SettingsApplyTests(unittest.TestCase):
    def test_failed_write_keeps_cvars_and_restores_layouts(self):
        if not shutil.which("g++"):
            self.skipTest("g++ required")
        source = (ROOT / "czm/menu/CzmMenu.cpp").read_text()
        function = re.search(r"void CZM_CloudFrame\( void \)\n\{.*?\n\}", source, re.S).group()
        vector = r'''
#pragma once
#include <vector>
template<class T> class CUtlVector {
 std::vector<T> v;
public:
 int Count() const{return v.size();}
 int AddToTail(const T &t){v.push_back(t);return v.size()-1;}
 void RemoveAll(){v.clear();}
 T &operator[](int n){return v[n];}
 const T &operator[](int n)const{return v[n];}
};
'''
        string = r'''
#pragma once
#include <string>
#include <cstdio>
class CUtlString {
 std::string s;
public:
 void Set(const char *x){s=x;}
 void SetDirect(const char *x,size_t n){s.assign(x,n);}
 const char *Get()const{return s.c_str();}
 int Length()const{return s.size();}
 void Append(const char *x){s+=x;}
 template<typename... A>void AppendFormat(const char *f,A... a){char b[64];snprintf(b,sizeof(b),f,a...);s+=b;}
};
'''
        harness = r'''
#include "CzmJson.h"
#include "czm_settings_names.h"
#include <map>
#include <string>
#include <cassert>
#include <cstring>
#include <cstdlib>
typedef unsigned char byte;
static std::map<std::string,std::string> files,cvars;
static std::string event,failPath;static bool fail,ackOk;static int ack,commands;
struct EngFuncs {
 static byte *COM_LoadFile(const char *p,int *n){auto i=files.find(p);if(i==files.end())return NULL;*n=i->second.size();byte *b=(byte *)malloc(*n+1);memcpy(b,i->second.c_str(),*n+1);return b;}
 static void COM_FreeFile(void *p){free(p);}
 static int COM_SaveFile(const char *p,const void *b,int n){if(fail&&failPath==p){fail=false;files[p]="partial";return 0;}files[p]=std::string((const char *)b,n);return 1;}
 static int DeleteFile(const char *p){return files.erase(p);}
 static void CvarSetString(const char *k,const char *v){cvars[k]=v;}
 static void ClientCmd(bool,const char *){commands++;}
};
static bool CZM_CloudEvent(CUtlString &out){if(event.empty())return false;out.Set(event.c_str());event.clear();return true;}
static void CZM_CloudApplied(int n,bool ok){ack=n;ackOk=ok;}
static int CZM_TouchScheme(){return 1;}
static void CZM_SetTouchScheme(int,bool){commands++;}
''' + function + r'''
int main(){
 std::string p1="touch_profiles/czm_scheme1.cfg",p2="touch_profiles/czm_scheme2.cfg";
 const char *valid=R"({"cloudId":7,"cvars":{"volume":"0.5","password":"never"},"files":{"1":"new1","2":"new2"}})";
 files[p1]="old1";files[p2]="old2";cvars["volume"]="1";event=valid;fail=true;failPath=p2;
 CZM_CloudFrame();assert(ack==7&&!ackOk&&commands==0);assert(files[p1]=="old1"&&files[p2]=="old2"&&cvars["volume"]=="1");
 files.erase(p2);event=valid;fail=true;CZM_CloudFrame();assert(!files.count(p2)&&files[p1]=="old1"&&commands==0);
 event=valid;CZM_CloudFrame();assert(ackOk&&commands==2&&cvars["volume"]=="0.5"&&!cvars.count("password"));assert(files[p1]=="new1"&&files[p2]=="new2");
 event=R"({"cloudId":8,"cvars":[],"files":{}})";CZM_CloudFrame();assert(ack==8&&!ackOk&&commands==2);
 ack=0;event=R"({"cvars":{},"files":{"1":"import"}})";CZM_CloudFrame();assert(ack==0&&files[p1]=="import");
}
'''
        with tempfile.TemporaryDirectory() as temp:
            folder = Path(temp)
            (folder / "utlvector.h").write_text(vector)
            (folder / "utlstring.h").write_text(string)
            (folder / "main.cpp").write_text(harness)
            exe = folder / "apply"
            subprocess.run(["g++", "-std=c++11", "-I"+temp, "-I"+str(ROOT/"czm/menu"),
                            "-I"+str(ROOT/"common"), str(folder/"main.cpp"),
                            str(ROOT/"czm/menu/CzmJson.cpp"), "-o", str(exe)], check=True)
            subprocess.run([str(exe)], check=True)


if __name__ == "__main__":
    unittest.main()
