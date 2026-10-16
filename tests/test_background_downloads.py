"""Execute engine background branches: downloads continue, GL preparation waits."""
from pathlib import Path
import subprocess
import tempfile
import unittest
from test_loading_errors import function

ROOT = Path(__file__).resolve().parents[1]
ENGINE = ROOT / "3rdparty/xash3d-fwgs/engine/client"


class BackgroundDownloadsTests(unittest.TestCase):
    def test_bytes_progress_and_graphics_wait_for_foreground(self):
        source = (ENGINE / "cl_main.c").read_text()
        loading = (ENGINE / "czm_loading.c").read_text()
        precache = function(source, "qboolean CL_PrecacheResources(")
        precache = precache[:precache.index("\t// if we downloaded")] + "return true;\n}\n"
        frame = function(source, "void Host_ClientFrame(")
        frame = frame[:frame.index("\tif( cls.key_dest")] + "rendered++;\n}\n"
        harness = r'''
#include <cassert>
#define __ANDROID__ 1
typedef bool qboolean;
struct resource_t {resource_t *pNext;};
enum {HOST_FRAME,HOST_SLEEP,ca_validate,key_game,event_up};
struct {int status;}host;
struct {bool initialized;int state,key_dest;}cls;
struct {resource_t resourcesneeded;}cl;
struct {bool active,files_ready;}czm;
struct {float value;}czm_loading_stage;
#define CZM_STAGE_DOWNLOAD 3
#define CZM_CANCEL_X0 0.815f
#define CZM_CANCEL_X1 0.955f
#define CZM_CANCEL_Y0 0.818f
#define CZM_CANCEL_Y1 0.898f
static bool requested,czm_deferred_precache;static int ready,rendered,acks,packets,retries,slept,registered,cancelled,background;
static bool CZM_AndroidFlag(const char *){return requested;}
static void CZM_AndroidProgress(int status){assert(status==3);ready++;}
static void CZM_Cancel(){cancelled++;}
static void CZM_AndroidBackground(){background++;}
''' + function(loading, "qboolean CZM_Loading_WaitForForeground(") + "\n"
        harness += function(loading, "void CZM_Loading_FilesReady(") + "\n" + precache
        harness += r'''
static bool CZM_Loading_Active(){return czm.active;}
static void CL_SendCommand(){acks++;}
static void CL_ReadPackets(){packets++;}
static void CL_CheckForResend(){retries++;}
static bool CL_RequestMissingResources(){return false;}
static void SCR_UpdateScreen(){}
static void Platform_Sleep(int ms){slept+=ms;}
static void CL_BatchResourceRequest(bool){if(CL_PrecacheResources())registered++;}
''' + frame + function(loading, "qboolean CZM_Loading_TouchEvent(") + r'''
int main(){
 cls.initialized=true;cls.state=ca_validate;cls.key_dest=key_game;czm.active=true;host.status=HOST_SLEEP;
 cl.resourcesneeded.pNext=&cl.resourcesneeded;
 assert(!CL_PrecacheResources()&&czm_deferred_precache&&ready==1);
 Host_ClientFrame();assert(acks==1&&packets==1&&retries==1&&rendered==0&&registered==0);
 host.status=HOST_FRAME;requested=true;assert(!CL_PrecacheResources());Host_ClientFrame();assert(registered==0);
 requested=false;Host_ClientFrame();assert(registered==1&&rendered==2&&!czm_deferred_precache);
 resource_t missing;cl.resourcesneeded.pNext=&missing;host.status=HOST_SLEEP;int was=ready;
 assert(!CL_PrecacheResources()&&ready==was);czm.active=false;Host_ClientFrame();assert(slept==100&&packets==1);
 czm.active=true;czm_loading_stage.value=3;assert(CZM_Loading_TouchEvent(event_up,.7f,.85f)&&background==1&&cancelled==0);
 CZM_Loading_TouchEvent(event_up,.88f,.85f);assert(cancelled==1&&background==1);
 czm_loading_stage.value=4;CZM_Loading_TouchEvent(event_up,.7f,.85f);assert(background==1);
}
'''
        with tempfile.TemporaryDirectory() as temporary:
            folder = Path(temporary)
            (folder / "check.cpp").write_text(harness)
            exe = folder / "check"
            subprocess.run(["g++", "-std=c++11", str(folder / "check.cpp"), "-o", str(exe)], check=True)
            subprocess.run([str(exe)], check=True)


if __name__ == "__main__":
    unittest.main()
