//
// tests.h
//
// Shared by the direct display driver's PC tests (see test.cpp).
//
// This file is part of BMDOS, a bare metal MS-DOS PC emulator for
// the Raspberry Pi. See LICENSE for licensing details.
//

#ifndef BMDOS_TOOLS_PI4KMS_TESTS_H_
#define BMDOS_TOOLS_PI4KMS_TESTS_H_

extern int g_failed;
void Check(bool ok, const char *name, const char *what);

// test_modes.cpp: EDID, video modes, InfoFrames, PHY settings.
void RunModeTests(void);

#endif
