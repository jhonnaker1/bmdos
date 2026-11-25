//
// corepool.h
//
// Work split in parts, shared between the core that has it (core 1, the
// emulator's) and helper cores that would otherwise be idle: DOSBox
// Pure's 3dfx Voodoo hands each big triangle's pixels over in parts
// (host_pool_run(), src/host.h). The emulator's own threads are
// cooperative, on core 1 alone (src/port/circle/threads.c), so this is the
// only way its work reaches the other cores.
//
// Run() posts the parts, takes some itself, and returns once every part
// is done and every helper registered then has finished with them, so
// between runs no helper touches anything. Parts are taken one at a time
// from a shared counter (an atomic add), so the number of helpers, or a
// late one, doesn't matter. Helpers register once, at start-up, before the
// first run; with none, Run() does every part itself.
//
// After src/av/frameshare.h (the same handshake for smoothed frames).
//
// Sources:
//  - GCC, "Built-in Functions for Memory Model Aware Atomic Operations".
//  - Arm Architecture Reference Manual (Armv8-A): WFE waits for an event,
//    SEV sends one to every core; a DSB first makes the writes visible.
//  - Circle, include/circle/synchronize64.h.
//
// This file is part of BMDOS, a bare metal MS-DOS PC emulator for
// the Raspberry Pi. See LICENSE for licensing details.
//

#ifndef BMDOS_COREPOOL_H_
#define BMDOS_COREPOOL_H_

#include <circle/synchronize.h>

class CorePool {
public:
  typedef void (*Part)(void *arg, unsigned part);

  CorePool(void)
      : m_nGeneration(0), m_nHelpers(0), m_nFinished(0), m_nNext(0), m_nDone(0),
        m_nTotal(0), m_pPart(0), m_pArg(0) {}

  unsigned Helpers(void) { return Load(&m_nHelpers); }

  // The owning core: runs PART for 0 to PARTS - 1 with whatever helpers
  // there are.
  void Run(Part part, void *arg, unsigned parts) {
    unsigned helpers = Load(&m_nHelpers);
    if (helpers == 0 || parts < 2) {
      for (unsigned i = 0; i < parts; i++) {
        part(arg, i);
      }
      return;
    }
    m_pPart = part;
    m_pArg = arg;
    Store(&m_nFinished, 0);
    Store(&m_nDone, 0);
    Store(&m_nNext, 0);
    Store(&m_nTotal, parts);
    Add(&m_nGeneration, 1);
    Signal();
    Share();
    while (Load(&m_nDone) < parts || Load(&m_nFinished) < helpers) {
      Wait();
    }
  }

  // A helper core, once: from now on it helps with every run. Returns the
  // generation it has seen, for Help().
  unsigned Register(void) {
    // Read before registering, so a run posted after this helper is
    // counted is one it sees as new.
    unsigned seen = Load(&m_nGeneration);
    Add(&m_nHelpers, 1);
    Signal();
    return seen;
  }

  // A helper core: does its share of a run posted since SEEN, if there is
  // one (true), else returns false at once (the caller waits for an event).
  bool Help(unsigned &seen) {
    unsigned generation = Load(&m_nGeneration);
    if (generation == seen) {
      return false;
    }
    seen = generation;
    Share();
    Add(&m_nFinished, 1);
    Signal();
    return true;
  }

  // A helper core with nothing else to do. Never returns.
  void HelpForever(void) {
    unsigned seen = Register();
    for (;;) {
      if (!Help(seen)) {
        Wait();
      }
    }
  }

private:
  void Share(void) {
    Part part = m_pPart;
    void *arg = m_pArg;
    unsigned total = Load(&m_nTotal);
    for (;;) {
      unsigned i = Add(&m_nNext, 1);
      if (i >= total) {
        break;
      }
      part(arg, i);
      Add(&m_nDone, 1);
      Signal();
    }
  }

  static unsigned Add(volatile unsigned *p, unsigned n) {
    return __atomic_fetch_add(p, n, __ATOMIC_ACQ_REL);
  }
  static unsigned Load(volatile unsigned *p) {
    return __atomic_load_n(p, __ATOMIC_ACQUIRE);
  }
  static void Store(volatile unsigned *p, unsigned v) {
    __atomic_store_n(p, v, __ATOMIC_RELEASE);
  }
  static void Wait(void) { WaitForEvent(); }
  static void Signal(void) {
    DataSyncBarrier();
    SendEvent();
  }

  volatile unsigned m_nGeneration;
  volatile unsigned m_nHelpers;
  volatile unsigned m_nFinished;
  volatile unsigned m_nNext, m_nDone, m_nTotal;
  Part volatile m_pPart;
  void *volatile m_pArg;
};

// The one pool (src/kernel.cpp).
CorePool &TheCorePool(void);

#endif
