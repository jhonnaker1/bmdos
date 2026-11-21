/*
 * biosfix.cpp - default handlers for IRQ 13, 14 and 15 in DOSBox's BIOS.
 *
 * A PC's BIOS points every hardware interrupt's vector at a handler. DOSBox
 * Pure's leaves INT 75h, 76h and 77h (IRQ 13, the FPU; IRQ 14 and 15, the
 * primary and secondary IDE controllers) at 0000:0000: DOSBox gives default
 * handlers only to INT 00h-5Fh and 68h-6Fh (src/cpu/callback.cpp), and its
 * BIOS sets IRQ 8 to 12's (src/ints/bios.cpp). Its IDE emulation (from
 * DOSBox-X) does raise IRQ 14 and 15 when a program enables the drive's
 * interrupt. FreeDOS's kernel hooks these vectors for its interrupt stacks
 * and chains to what was there: so FreeDOS's CD-ROM drivers UDVD2 and UIDE,
 * which send the CD drive commands with its interrupt on, sent the PC to
 * 0000:0000 at the drive's first interrupt, and FreeDOS stopped while it
 * started (its CDROM.BAT). A driver that polls with the interrupt off
 * (Samsung's SSCDROM.SYS under MS-DOS 6.22) never met it.
 *
 * Each time DOSBox starts, BMDOS points the vectors that are still 0000:0000
 * at a handler of its own, as a real BIOS's does: it acknowledges the
 * interrupt at both interrupt controllers and returns. The handler's 9 bytes
 * of code are in a callback slot of DOSBox's BIOS, so they are in the BIOS's
 * memory and survive a booted DOS. DOSBox Pure itself is not changed.
 *
 * This runs between frames, so a program DOSBox Pure starts in its very
 * first frame (a ZIP with just one program in it) could meet the empty
 * vectors still; DOSBox Pure's start menu, or a DOS booted from an image,
 * leaves time for them to be set.
 *
 * This file is part of BMDOS, a bare metal MS-DOS PC emulator for the
 * Raspberry Pi. See LICENSE for licensing details.
 */

#include "dosbox.h"
#include "callback.h"
#include "mem.h"
#include "serialport.h"

#include <stdio.h>

extern "C" void bmdos_biosfix_frame(void)
{
	static const Bit8u eoi_iret[] = {
		0x50,             /* push ax */
		0xb0, 0x20,       /* mov al,20h */
		0xe6, 0xa0,       /* out 0A0h,al: EOI at the second controller */
		0xe6, 0x20,       /* out 20h,al: and at the first */
		0x58,             /* pop ax */
		0xcf,             /* iret */
	};
	static const Bit8u vectors[] = {0x75, 0x76, 0x77};
	static Bitu callback;
	static RealPt handler;
	bool any = false;
	unsigned i;
	/* DOSBox Pure always puts its modem in COM1, so serialports[0] is set
	   once DOSBox (its memory, BIOS and callbacks) is set up. */
	if (serialports[0] == NULL)
		return;
	for (i = 0; i < sizeof vectors; i++)
		if (mem_readd(vectors[i] * 4) == 0)
			any = true;
	if (!any)
		return;
	/* A DOSBox started again has its callbacks set up again too: our
	   handler is written to a slot of its own each time the vectors are
	   found empty (the slot's code is checked, in case it was reused). */
	if (handler == 0 || mem_readb(RealSeg(handler) * 16 + RealOff(handler)) != eoi_iret[0]
	    || mem_readb(RealSeg(handler) * 16 + RealOff(handler) + 8) != eoi_iret[8]) {
		callback = CALLBACK_Allocate();
		PhysPt p = CALLBACK_PhysPointer(callback);
		for (i = 0; i < sizeof eoi_iret; i++)
			phys_writeb(p + i, eoi_iret[i]);
		handler = CALLBACK_RealPointer(callback);
	}
	for (i = 0; i < sizeof vectors; i++)
		if (mem_readd(vectors[i] * 4) == 0)
			RealSetVec(vectors[i], handler);
	printf("BMDOS: default handlers for IRQ 13, 14 and 15 at %04x:%04x\n", RealSeg(handler),
	       RealOff(handler));
}
