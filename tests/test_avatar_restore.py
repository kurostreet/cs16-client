"""A missing account photo may arrive later under the very same cached filename."""
import re
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class AvatarRestoreTests(unittest.TestCase):
    def test_missing_photo_can_become_available_without_renaming(self):
        if not shutil.which("g++"):
            self.skipTest("requires g++")
        source = (ROOT / "czm/menu/CzmMenu.cpp").read_text()
        function = re.search(r"const char \*CZM_AvatarPhoto\( void \)\n\{.*?\n\}", source, re.S).group()
        harness = r'''
#include <cstring>
#include <cassert>
static const char *configured = "czm_cache/account_7_1.png";
static bool available;
static void Q_strncpy(char *out, const char *s, size_t n) { strncpy(out,s,n);out[n-1]=0; }
struct EngFuncs {
 static const char *GetCvarString(const char *) { return configured; }
 static int FileExists(const char *, bool) { return available; }
};
''' + function + r'''
int main() {
 assert(!CZM_AvatarPhoto()[0]);
 available=true;
 assert(!strcmp(CZM_AvatarPhoto(),configured));
 configured="czm_cache/account_8_2.png";available=false;
 assert(!CZM_AvatarPhoto()[0]);
 available=true;
 assert(!strcmp(CZM_AvatarPhoto(),configured));
 configured="";
 assert(!CZM_AvatarPhoto()[0]);
}
'''
        with tempfile.TemporaryDirectory() as temp:
            cpp = Path(temp)/"restore.cpp"; cpp.write_text(harness)
            exe = Path(temp)/"restore"
            subprocess.run(["g++", "-std=c++11", str(cpp), "-o", str(exe)], check=True)
            subprocess.run([str(exe)], check=True)


if __name__ == "__main__":
    unittest.main()
