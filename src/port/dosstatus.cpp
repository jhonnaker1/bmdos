/*
 * dosstatus.cpp - what DOSBox's PC is doing, for the log.
 *
 * BMDOS logs a line every 10 s (platform.c, report_speed()); this adds the
 * PC's side to it: the CPU core running (DOSBox Pure's "auto" switches to
 * the dynamic recompiler once a program runs with automatic cycles, even
 * in real mode), its cycles a millisecond, the PC's clock (PIC_Ticks, the
 * milliseconds DOSBox has emulated: if it doesn't move, nothing that waits
 * for the timer can go on), the mode and where the CPU is. Read between
 * frames, when DOSBox Pure's thread is waiting.
 *
 * This file is part of BMDOS, a bare metal MS-DOS PC emulator for the
 * Raspberry Pi. See LICENSE for licensing details.
 */

#include "dosbox.h"
#include "cpu.h"
#include "pic.h"
#include "regs.h"

#include <stdio.h>

extern "C" void bmdos_dosbox_status(char *text, size_t size)
{
	const char *core = "other";
	if (cpudecoder == &CPU_Core_Normal_Run)
		core = "normal";
	else if (cpudecoder == &CPU_Core_Simple_Run)
		core = "simple";
#if C_DYNREC
	else if (cpudecoder == &CPU_Core_Dynrec_Run)
		core = "dynamic";
#endif
	snprintf(text, size, "core %s, %d cycles/ms%s, PC clock %lu ms, %s mode at %04x:%08x", core,
	         (int) CPU_CycleMax, CPU_CycleAutoAdjust ? " (max)" : "", (unsigned long) PIC_Ticks,
	         cpu.pmode ? (GETFLAG(VM) ? "V86" : "protected") : "real", (unsigned) SegValue(cs),
	         (unsigned) reg_eip);
}
