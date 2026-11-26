//
// display.cpp
//
// See display.h.
//
// This file is part of BMDOS, a bare metal MS-DOS PC emulator for
// the Raspberry Pi. See LICENSE for licensing details.
//

#include "display.h"

#include <circle/bcm2835.h>
#include <circle/logger.h>
#include <circle/memory.h>
#include <circle/new.h>
#include <circle/sched/scheduler.h>
#include <circle/synchronize.h>
#include <circle/timer.h>
#include <string.h>

namespace kms {

static const char FromDisplay[] = "kms";

// A frame takes at most 1/24 s; more than this and the display has gone.
static const unsigned FrameTimeoutMs = 100;

Display::Display(void)
    : m_bAttached(false), m_nPitch(0), m_nBufferWords(0), m_nBack(0),
      m_nBuffers(0), m_nShown(0), m_nQueued(0),
      m_bFlipPending(false), m_nFlipCount(0), m_bHavePicture(false),
      m_bTimedOut(false) {
  for (unsigned i = 0; i < MaxBuffers; i++) {
    m_pBuffer[i] = 0;
  }
  memset(&m_Plane, 0, sizeof m_Plane);
}

Display::~Display(void) { FreeBuffers(); }

void Display::FreeBuffers(void) {
  for (unsigned i = 0; i < MaxBuffers; i++) {
    if (m_pBuffer[i] != 0) {
      CMemorySystem::HeapFree(m_pBuffer[i]);
      m_pBuffer[i] = 0;
    }
  }
  m_nBufferWords = 0;
}

bool Display::Attach(void) {
  if (!m_Hvs.Attach()) {
    CLogger::Get()->Write(FromDisplay, LogWarning,
                          "Neither HDMI port is fed by the HVS; using the firmware's framebuffer");
    return false;
  }
  m_ModeSetter.SetPort(m_Hvs.Port());
  m_bAttached = true;
  CLogger::Get()->Write(FromDisplay, LogNotice, "HDMI%u, HVS channel %u, %ux%u",
                        m_Hvs.Port(), m_Hvs.Channel(), m_Hvs.Width(), m_Hvs.Height());
  return true;
}

bool Display::SetPicture(unsigned src_w, unsigned src_h, unsigned x, unsigned y,
                         unsigned w, unsigned h, bool nearest) {
  if (!m_bAttached || src_w == 0 || src_h == 0) {
    return false;
  }
  // Lines start on 64-byte (cache line) boundaries.
  unsigned pitch = (src_w + 15) & ~15u;
  unsigned words = pitch * src_h;
  if (words > m_nBufferWords) {
    // The buffers may be on screen: show the background until the new ones
    // are drawn.
    unsigned now = m_Hvs.FrameCount();
    m_Hvs.Show(0);
    if (m_Hvs.WaitForChange(now, FrameTimeoutMs)) {
      m_Hvs.WaitForChange((now + 1) & 63, FrameTimeoutMs);
    }
    FreeBuffers();
    m_nBuffers = MaxBuffers;
    for (unsigned i = 0; i < m_nBuffers; i++) {
      m_pBuffer[i] = (uint32_t *)CMemorySystem::HeapAllocate(words * 4, HEAP_DMA30);
      if (m_pBuffer[i] == 0) {
        FreeBuffers();
        m_bHavePicture = false;
        CLogger::Get()->Write(FromDisplay, LogError, "Not enough memory for the picture");
        return false;
      }
      memset(m_pBuffer[i], 0, words * 4);
      CleanAndInvalidateDataCacheRange((uintptr)m_pBuffer[i], words * 4);
    }
    m_nBufferWords = words;
    m_nShown = m_nQueued = m_nBack = 0;
    m_bFlipPending = false;
    m_Plane.bus_address = 0;   // nothing drawn into the new buffers yet
  }
  m_nPitch = pitch;
  m_Plane.pitch = pitch * 4;
  m_Plane.src_width = src_w;
  m_Plane.src_height = src_h;
  m_Plane.x = x;
  m_Plane.y = y;
  m_Plane.width = w;
  m_Plane.height = h;
  m_Plane.nearest = nearest;
  // The HVS must be able to show it (a trial list), with line buffer
  // memory to spare.
  uint32_t trial[MaxWords];
  m_bHavePicture = LbmWords(m_Plane) <= 28 * 1024 && BuildList(m_Plane, 0, 0, trial) != 0;
  CLogger::Get()->Write(FromDisplay, LogNotice, "Picture %ux%u shown at %u,%u %ux%u, %s",
                        src_w, src_h, x, y, w, h, nearest ? "sharp" : "smooth");
  return m_bHavePicture;
}

uint32_t *Display::BeginFrame(unsigned *pitch) {
  if (!m_bHavePicture) {
    return 0;
  }
  NoteFlip();
  if (m_nBuffers < 3 && m_bFlipPending) {
    // Two buffers: the other one is on screen until the frame after the
    // last flip.
    if (!WaitForFlip(m_nFlipCount) && !m_bTimedOut) {
      m_bTimedOut = true;
      CLogger::Get()->Write(FromDisplay, LogWarning, "The HVS has stopped (display off?)");
    }
    m_nShown = m_nQueued;
    m_bFlipPending = false;
  }
  // A buffer neither on screen nor waiting to be: with three there always
  // is one, so a late frame doesn't wait for the vertical blank.
  for (unsigned i = 0; i < m_nBuffers; i++) {
    if (i != m_nShown && !(m_bFlipPending && i == m_nQueued)) {
      m_nBack = i;
      break;
    }
  }
  *pitch = m_nPitch;
  return m_pBuffer[m_nBack];
}

void Display::EndFrame(bool wait_vsync, unsigned first_line, unsigned lines) {
  if (!m_bHavePicture) {
    return;
  }
  uint32_t *buffer = m_pBuffer[m_nBack];
  if (first_line < m_Plane.src_height) {
    if (lines > m_Plane.src_height - first_line) {
      lines = m_Plane.src_height - first_line;
    }
    if (lines != 0) {
      CleanAndInvalidateDataCacheRange((uintptr)(buffer + (size_t)first_line * m_nPitch),
                                       (size_t)m_nPitch * lines * 4);
    }
  }
  m_Plane.bus_address = (uint32_t)BUS_ADDRESS((uintptr)buffer);
  // A buffer queued before may have reached the screen while this one was
  // drawn: it is the one on screen then, not free.
  NoteFlip();
  unsigned now = m_Hvs.FrameCount();
  if (!m_Hvs.Show(&m_Plane)) {
    return;
  }
  // The frame being composed now still shows the buffer before; from the
  // next one on, this one (a buffer queued before and never shown is just
  // free again).
  // Late: the vertical blank this frame was for (the one after the frame
  // before was queued) has passed already. It is shown at the next one
  // without waiting for it, or it would take two frames' time (the caller
  // can't tell beforehand how long drawing it takes).
  const bool late = ((now - m_nFlipCount) & 63) >= 2;
  m_nQueued = m_nBack;
  m_bFlipPending = true;
  m_nFlipCount = now;
  if (wait_vsync && !late) {
    if (WaitForFlip(now)) {
      m_nShown = m_nQueued;
      m_bFlipPending = false;
    }
  }
}

void Display::NoteFlip(void) {
  if (m_bFlipPending && m_Hvs.FrameCount() != m_nFlipCount) {
    m_nShown = m_nQueued;
    m_bFlipPending = false;
  }
}

// Circle's tasks (the network, Wi-Fi, the web UI) only run when core 1
// yields, so the wait for the next frame runs them, as the Pi 3's wait in
// vc_dispmanx_update_submit_sync() does (Circle's addon/linux semaphores
// yield while they wait). Spinning instead leaves them one turn a frame,
// which is far too little for the Wi-Fi driver.
uint64_t Display::s_nWaitTicks;

bool Display::WaitForFlip(unsigned from) {
  u64 start = CTimer::GetClockTicks64();
  u64 limit = (u64)FrameTimeoutMs * (CLOCKHZ / 1000);
  bool flipped = true;
  while (m_Hvs.FrameCount() == from) {
    if (CTimer::GetClockTicks64() - start > limit) {
      flipped = false;
      break;
    }
    CScheduler::Get()->Yield();
  }
  s_nWaitTicks += CTimer::GetClockTicks64() - start;
  return flipped;
}

bool Display::MatchRate(double fps, unsigned audio_fs) {
  if (!m_bAttached) {
    return false;
  }
  Timing current, wanted;
  if (!m_ModeSetter.Current(&current)) {
    return false;
  }
  if (!ChooseMode(current, fps, m_ModeSetter.GetEdid(), &wanted)) {
    CLogger::Get()->Write(FromDisplay, LogNotice,
                          "No %ux%u mode at %.2f Hz the display lists; keeping %.2f Hz",
                          current.h_active, current.v_active, fps, current.Refresh());
    return false;
  }
  if (wanted.vic == current.vic &&
      (wanted.pixel_hz > current.pixel_hz ? wanted.pixel_hz - current.pixel_hz
                                          : current.pixel_hz - wanted.pixel_hz) < 1000) {
    return false;
  }
  m_ModeSetter.Set(m_Hvs, wanted, audio_fs, Reinstall, this);
  return true;
}

void Display::Reinstall(void *param) {
  Display *display = (Display *)param;
  // The buffer last shown is the front one.
  if (display->m_bHavePicture && display->m_Plane.bus_address != 0) {
    display->m_Hvs.Show(&display->m_Plane);
  } else {
    display->m_Hvs.Show(0);
  }
  display->m_nShown = display->m_nQueued;
  display->m_bFlipPending = false;
}

void Display::Blank(void) {
  if (m_bAttached) {
    m_Hvs.Show(0);
  }
}

void Display::Release(void) {
  if (m_bAttached) {
    m_Hvs.Restore();
  }
}

} // namespace kms
