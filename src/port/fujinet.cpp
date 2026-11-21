/*
 * fujinet.cpp - a FujiNet on the PC's serial port, through FujiNet-PC.
 *
 * FujiNet (https://fujinet.online/) has an RS-232 version for the IBM PC:
 * a FujiNet on a COM port, which MS-DOS reaches with its driver and tools
 * (fujinet-msdos: FUJINET.SYS mounts its disks as DOS drives, FMOUNT,
 * NGET, FNSHARE...). FujiNet-PC, FujiNet's program for a computer, built
 * for that target (fujinet-firmware's build.sh -p RS232), carries its end
 * of the serial line over TCP ("BoIP", lib/hardware/BoIPChannel.cpp): with
 * [BOIP] enabled=1 in its fnconfig.ini it listens (on the RS-232 bus BoIP
 * is a server: lib/bus/rs232/rs232.cpp), on port 1985 by default and only
 * on the computer's own loopback address unless its host is set (host=*,
 * or the computer's address). BMDOS connects to it there (fujinet_host,
 * fujinet_port), as FujiNet Go MS-DOS's 86Box does, and again every few
 * seconds while it can't (unlike BMApple2's FujiNet, which FujiNet-PC
 * connects to).
 *
 * The bytes are the serial line's, as they are: FujiNet's SLIP-framed
 * FujiBus packets, at whatever speed the PC set (FUJINET.SYS uses
 * 115200). Over BoIP FujiNet doesn't use the modem lines, so here DSR and
 * CTS are always on, and DCD while FujiNet-PC is connected.
 *
 * The port is a DOSBox CSerial of BMDOS's own (DOSBox Pure is not changed):
 * each time DOSBox starts, BMDOS puts it in COM1 or COM2 (fujinet_com) in
 * place of what DOSBox Pure put there (its libretro modem on COM1, which
 * needs a frontend's netplay, which BMDOS has none of). The connection to
 * FujiNet-PC outlives DOSBox's restarts. The UART's timing (bytes taking
 * their time to go out and come in) follows DOSBox's null modem.
 *
 * This file is part of BMDOS, a bare metal MS-DOS PC emulator for the
 * Raspberry Pi. See LICENSE for licensing details.
 */

#include "dosbox.h"
#include "serialport.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
#include "../host.h"
const char *host_option(const char *name, const char *default_value);
}

/* FujiNet-PC's BoIP port for the RS-232 build (fujinet-firmware's
   CONFIG_DEFAULT_BOIP_PORT, lib/config/fnConfig.h, for targets other than
   the Atari, CoCo, ADAM and IEC ones). */
#define FUJINET_DEFAULT_PORT "1985"

namespace {

int connection = -1;     /* FujiNet-PC's, -1 none */
volatile int connecting; /* a background connect is under way */
volatile int connect_result = -1;
char host[128], port[8];
int enabled = -1;        /* fujinet= (read once) */
int com = 1;             /* fujinet_com= */
unsigned long long bytes_in, bytes_out;

/* What has come from FujiNet-PC and not yet into the UART, and what the
   UART has sent and not yet gone out. */
enum { RING = 8192 };
unsigned char rx_ring[RING];
unsigned rx_head, rx_tail;
unsigned char tx_buf[RING];
unsigned tx_len;

void drop_connection(const char *why)
{
	if (connection < 0)
		return;
	printf("BMDOS: FujiNet-PC %s (%llu bytes in, %llu out)\n", why, bytes_in, bytes_out);
	host_tcp_close(connection);
	connection = -1;
	rx_head = rx_tail = tx_len = 0;
}

/* Sends what the UART has sent; reads what has come in, as far as the
   ring has room. */
void pump(void)
{
	if (connection < 0) {
		tx_len = 0;
		return;
	}
	if (tx_len > 0) {
		int sent = host_tcp_send(connection, tx_buf, tx_len);
		if (sent < 0) {
			drop_connection("closed the connection");
			return;
		}
		if (sent > 0) {
			memmove(tx_buf, tx_buf + sent, tx_len - (unsigned) sent);
			tx_len -= (unsigned) sent;
			bytes_out += (unsigned) sent;
		}
	}
	for (;;) {
		unsigned char buf[512];
		unsigned room = RING - 1 - (rx_head - rx_tail) % RING, n;
		int got;
		if (room == 0)
			break;
		got = host_tcp_recv(connection, buf, room < sizeof buf ? room : sizeof buf);
		if (got < 0) {
			drop_connection("closed the connection");
			return;
		}
		if (got == 0)
			break;
		for (n = 0; n < (unsigned) got; n++) {
			rx_ring[rx_head % RING] = buf[n];
			rx_head++;
		}
		bytes_in += (unsigned) got;
	}
}

class CFujiNetSerial;
CFujiNetSerial *installed;

class CFujiNetSerial : public CSerial {
public:
	CFujiNetSerial(Bitu id, CommandLine *cmd) : CSerial(id, cmd), rx_active(false)
	{
		InstallationSuccessful = true;
		CSerial::Init_Registers();
		setDSR(true);
		setCTS(true);
		setRI(false);
		setCD(connection >= 0);
		setEvent(SERIAL_POLLING_EVENT, 1.0f);
	}

	~CFujiNetSerial()
	{
		removeEvent(SERIAL_POLLING_EVENT);
		removeEvent(SERIAL_RX_EVENT);
		removeEvent(SERIAL_TX_EVENT);
		removeEvent(SERIAL_THR_EVENT);
		if (installed == this)
			installed = NULL;
	}

	void handleUpperEvent(Bit16u type)
	{
		switch (type) {
		case SERIAL_POLLING_EVENT:
			pump();
			setCD(connection >= 0);
			if (!rx_active && rx_head != rx_tail) {
				rx_active = true;
				setEvent(SERIAL_RX_EVENT, bytetime * 0.9f);
			}
			setEvent(SERIAL_POLLING_EVENT, 1.0f);
			break;
		case SERIAL_RX_EVENT:
			if (rx_head == rx_tail) {
				rx_active = false;
				break;
			}
			if (CanReceiveByte()) {
				receiveByteEx(rx_ring[rx_tail % RING], 0);
				rx_tail++;
				setEvent(SERIAL_RX_EVENT, bytetime * 0.9f);
			} else {
				/* The program hasn't read the last yet: try again. */
				setEvent(SERIAL_RX_EVENT, bytetime * 0.65f);
			}
			break;
		case SERIAL_THR_EVENT:
			ByteTransmitting();
			setEvent(SERIAL_TX_EVENT, bytetime + 0.01f);
			break;
		case SERIAL_TX_EVENT:
			ByteTransmitted();
			break;
		}
	}

	void transmitByte(Bit8u val, bool first)
	{
		if (first)
			setEvent(SERIAL_THR_EVENT, bytetime / 8);
		else
			setEvent(SERIAL_TX_EVENT, bytetime);
		if (connection >= 0 && tx_len < sizeof tx_buf)
			tx_buf[tx_len++] = val;
	}

	void updateMSR() {}
	void setRTSDTR(bool rts, bool dtr) { (void) rts; (void) dtr; }
	void setRTS(bool val) { (void) val; }
	void setDTR(bool val) { (void) val; }
	void setBreak(bool value) { (void) value; }
	void updatePortConfig(Bit16u divider, Bit8u lcr) { (void) divider; (void) lcr; }

private:
	bool rx_active;
};

}  // namespace

extern "C" {

int bmdos_fujinet_enabled(void)
{
	if (enabled < 0) {
		const char *v = host_option("fujinet", "0");
		enabled = strcmp(v, "1") == 0 || strcmp(v, "on") == 0 || strcmp(v, "yes") == 0
		          || strcmp(v, "true") == 0;
		com = atoi(host_option("fujinet_com", "1"));
		if (com < 1 || com > 2)
			com = 1;
		snprintf(host, sizeof host, "%s", host_option("fujinet_host", ""));
		snprintf(port, sizeof port, "%s", host_option("fujinet_port", FUJINET_DEFAULT_PORT));
		if (atoi(port) <= 0 || atoi(port) > 65535)
			snprintf(port, sizeof port, "%s", FUJINET_DEFAULT_PORT);
		if (host[0] == '\0')
			enabled = 0;   /* nowhere to connect to */
	}
	return enabled;
}

/* host_tcp_connect() waits for the far end: in the background. */
static void connect_task(void *arg)
{
	(void) arg;
	connect_result = host_tcp_connect(host, port);
	__sync_synchronize();
	connecting = 0;
}

/* Once a frame, between DOSBox's frames: connects to FujiNet-PC (again
   every 5 s while it can't), and puts the port in COM1 or COM2 when DOSBox
   has (again) set up its serial ports. */
void bmdos_fujinet_frame(void)
{
	static double next_try;
	static int said_failed;
	int n;
	if (!bmdos_fujinet_enabled())
		return;
	if (connection < 0 && !connecting) {
		if (connect_result >= 0) {
			connection = connect_result;
			connect_result = -1;
			bytes_in = bytes_out = 0;
			said_failed = 0;
			printf("BMDOS: FujiNet-PC connected (%s:%s), on COM%d\n", host, port, com);
		} else if (host_time() >= next_try) {
			if (next_try > 0.0 && !said_failed++)
				printf("BMDOS: can't connect to FujiNet-PC at %s:%s; trying every 5 s\n",
				       host, port);
			next_try = host_time() + 5.0;
			connecting = 1;
			if (!host_run_in_background(connect_task, NULL))
				connecting = 0;
		}
	}
	/* DOSBox Pure always puts its modem in COM1 (serial1=libretro), so
	   serialports[0] is there once DOSBox's serial ports are set up. */
	n = com - 1;
	if (serialports[0] != NULL && (installed == NULL || serialports[n] != installed)) {
		CommandLine cmd(0, "");
		delete serialports[n];
		installed = new CFujiNetSerial((Bitu) n, &cmd);
		serialports[n] = installed;
		printf("BMDOS: FujiNet on COM%d\n", com);
	}
	if (installed == NULL)
		pump();   /* DOSBox not running: keep the connection read */
}

/* For the menu: what FujiNet is doing. */
void bmdos_fujinet_status(char *text, unsigned size)
{
	if (!bmdos_fujinet_enabled())
		snprintf(text, size, "Off");
	else if (connection >= 0)
		snprintf(text, size, "Connected to %s, on COM%d", host, com);
	else
		snprintf(text, size, "Connecting to %.40s:%s", host, port);
}

}  /* extern "C" */
