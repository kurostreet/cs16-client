"""Run the real patched capture state with the engine's real cvar protection code."""
import re
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ENGINE = ROOT / "3rdparty/xash3d-fwgs"


def function(source, signature):
    start = source.index(signature)
    opening = source.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


class MapCaptureTests(unittest.TestCase):
    def test_protected_state_spawn_capture_and_rejoin(self):
        screen = (ENGINE / "engine/client/cl_scrn.c").read_text()
        cvars = (ENGINE / "engine/common/cvar.c").read_text()
        declarations = "\n".join(re.findall(
            r"^static CVAR_DEFINE_AUTO\( czm_(?:current_map|map_visit|mapshot_ready|mapshot_state),[^\n]+", screen, re.M))
        self.assertEqual(len(declarations.splitlines()), 4)
        source = PREAMBLE + declarations + "\n"
        source += function(cvars, "static qboolean Cvar_CanSet(") + "\n"
        source += function(cvars, "void Cvar_DirectFullSet(") + "\n"
        source += SETTERS
        source += function(screen, "static void CZM_MapPictureState(") + "\n"
        source += function(screen, "void SCR_CzmMapPictureCapture(") + "\n"
        source += SCENARIO
        with tempfile.TemporaryDirectory(prefix="czm-map-capture-") as directory:
            path = Path(directory)
            (path / "capture.cpp").write_text(source)
            subprocess.run(["g++", "-std=c++11", str(path / "capture.cpp"), "-o", str(path / "check")], check=True)
            subprocess.run([str(path / "check")], check=True)


PREAMBLE = r'''
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
typedef bool qboolean;
enum { FCVAR_READ_ONLY = 1, FCVAR_PRIVILEGED = 2, FCVAR_CHEAT = 4, FCVAR_LATCH = 8 };
enum { ca_active = 1, HOST_FRAME = 1, scrshot_inactive = 0, VID_LEVELSHOT = 3 };
struct convar_t { const char *name; char *string; float value; uint32_t flags; };
#define CVAR_DEFINE_AUTO(n, s, f, d) convar_t n = { #n, strdup(s), 0, f }
#define FBitSet(f, b) ((f) & (b))
#define SetBits(f, b) ((f) |= (b))
#define Q_snprintf snprintf
#define Q_strcmp strcmp
#define Q_strlen strlen
#define Q_atof atof
#define Mem_Realloc(pool, ptr, size) ((char *)realloc(ptr, size))
static void Q_strncpy(char *out, const char *in, size_t n) { strncpy(out, in, n - 1); out[n - 1] = 0; }
static int read_only_errors;
static void Con_Printf(const char *, ...) { ++read_only_errors; }
static void Cvar_Changed(convar_t *) {}
static bool SV_Active() { return false; }
static bool CL_Active() { return false; }
static bool scr_init = true, menu_visible;
static struct { int state, spectator, scrshot_action; } cls;
static struct { int background, video_prepped; struct { int health; } local; } cl;
static struct { char mapname[64]; } clgame;
static struct { double realtime; int status; bool allow_cheats; } host;
static bool UI_IsVisible() { return menu_visible; }
static int capture_count;
static bool capture_ok = true;
static std::string captured_path;
static bool Capture(const char *path, int type) {
    assert(type == VID_LEVELSHOT);
    ++capture_count;
    captured_path = path;
    return capture_ok;
}
static struct { struct { bool (*VID_ScreenShot)(const char *, int); } dllFuncs; } ref = {{Capture}};
'''

SETTERS = r'''
// Public writes use the same permission guard as Cvar_DirectSet in the engine.
static void Cvar_DirectSet(convar_t *v, const char *s) {
    if (Cvar_CanSet(v)) Cvar_DirectFullSet(v, s, v->flags);
}
static void Cvar_DirectSetValue(convar_t *v, float value) {
    char s[32]; snprintf(s, sizeof(s), "%g", value); Cvar_DirectSet(v, s);
}
'''

SCENARIO = r'''
int main() {
    // The fix must retain protection from public commands and game servers.
    Cvar_DirectSet(&czm_current_map, "spoofed");
    assert(read_only_errors == 1 && !czm_current_map.string[0]);
    read_only_errors = 0;

    cls.state = ca_active; cl.video_prepped = 1; cl.local.health = 100;
    host.status = HOST_FRAME; host.realtime = 100;
    strcpy(clgame.mapname, "ZM_Battleground_Foda");
    CZM_MapPictureState();
    assert(!strcmp(czm_current_map.string, "zm_battleground_foda"));
    assert(czm_map_visit.value == 1 && czm_mapshot_ready.value == 0);
    host.realtime = 107.9; CZM_MapPictureState();
    assert(czm_mapshot_ready.value == 0);
    host.realtime = 108; CZM_MapPictureState();
    assert(czm_mapshot_ready.value == 1);
    for (int i = 0; i < 120; ++i) CZM_MapPictureState();
    assert(czm_map_visit.value == 1 && read_only_errors == 0);

    Cvar_DirectSetValue(&czm_mapshot_state, 1);
    menu_visible = true; SCR_CzmMapPictureCapture();
    assert(capture_count == 0 && czm_mapshot_state.value == 1);
    menu_visible = false; SCR_CzmMapPictureCapture();
    assert(capture_count == 1 && czm_mapshot_state.value == 2);
    assert(captured_path == "czm_cache/maps/zm_battleground_foda_capture.png");
    SCR_CzmMapPictureCapture();
    assert(capture_count == 1);

    cl.local.health = 0; CZM_MapPictureState();
    assert(czm_mapshot_ready.value == 0);
    host.realtime = 110; cl.local.health = 100; CZM_MapPictureState();
    assert(czm_mapshot_ready.value == 0);
    host.realtime = 118; CZM_MapPictureState();
    assert(czm_mapshot_ready.value == 1);
    capture_ok = false; Cvar_DirectSetValue(&czm_mapshot_state, 1);
    SCR_CzmMapPictureCapture();
    assert(capture_count == 2 && czm_mapshot_state.value == 3);

    // Disconnect must clear state; a rejoin to the same map is a new visit.
    cls.state = 0; CZM_MapPictureState();
    assert(!czm_current_map.string[0] && czm_mapshot_ready.value == 0 && czm_mapshot_state.value == 0);
    cls.state = ca_active; host.realtime = 130; CZM_MapPictureState();
    assert(czm_map_visit.value == 2 && czm_mapshot_ready.value == 0);
    host.realtime = 138; CZM_MapPictureState();
    assert(czm_mapshot_ready.value == 1);
    cls.spectator = 1; CZM_MapPictureState();
    assert(czm_mapshot_ready.value == 0);
    assert(czm_current_map.flags & FCVAR_READ_ONLY);
    assert(czm_map_visit.flags & FCVAR_READ_ONLY);
    assert(czm_mapshot_ready.flags & FCVAR_READ_ONLY);
    assert(read_only_errors == 0);
}
'''


if __name__ == "__main__":
    unittest.main()
