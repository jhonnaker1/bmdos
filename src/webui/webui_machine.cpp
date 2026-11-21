//
// webui_machine.cpp
//
// Modified for BMA800XL: the Atari 800XL's name, settings file and the
// file types atari800 can start, in place of BMC64's machines.
// Changed for BMCoco, BMApple2, BMAtariST and BMNeXT, and for BMDOS: DOS
// programs and disks.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "webui.h"

#if BMC64_WEBUI

#include "webui_machine.h"

#include <stdio.h>

// The machine running (src/port/platform.c).
extern "C" const char *bmdos_machine_text(void);

namespace {

// What DOSBox Pure can start (emu_autostart_interrupt() in
// src/port/platform.c loads it as the PC's content and starts the PC
// again): a game's ZIP or folder's program, a DOS program, a CD or floppy
// or hard disk image.
const char *const kRunTypes[] = {
    "zip", "dosz", "exe", "com", "bat", "iso", "cue", "chd", "img", "ima",
    "vhd", "jrc", "tc", "m3u", "conf", 0,
};

const WebUiMachine kMachine = {
    "/bmdos.cfg", kRunTypes,
};

char Lower(char c) {
  return (c >= 'A' && c <= 'Z') ? (char) (c - 'A' + 'a') : c;
}

}  // namespace

const WebUiMachine *WebUiMachineGet(void) {
  return &kMachine;
}

const char *WebUiMachineName(void) {
  return bmdos_machine_text();
}

boolean WebUiMachineCanRun(const char *ext) {
  for (const char *const *type = kMachine.run_types; *type != 0; type++) {
    const char *a = ext;
    const char *b = *type;
    while (*a != '\0' && Lower(*a) == *b) {
      a++;
      b++;
    }
    if (*a == '\0' && *b == '\0') return TRUE;
  }
  return FALSE;
}

int WebUiMachineCapsJson(char *out, unsigned size) {
  char run[128];
  unsigned used = 0;
  for (const char *const *type = kMachine.run_types; *type != 0; type++) {
    int n = snprintf(run + used, sizeof(run) - used, "%s\"%s\"",
                     used == 0 ? "" : ",", *type);
    if (n < 0 || (unsigned) n >= sizeof(run) - used) return -1;
    used += (unsigned) n;
  }
  run[used] = '\0';

  int length = snprintf(out, size,
                        "{\"settings\":\"%s\",\"run\":[%s]}",
                        kMachine.settings, run);
  if (length < 0 || (unsigned) length >= size) return -1;
  return length;
}

#endif  // BMC64_WEBUI
