#ifndef TRACK_REPORTER_H
#define TRACK_REPORTER_H

#include <Arduino.h>
#include "Config.h"

// ===================================================================
// === TrackReporterS88 ==============================================
// ===================================================================

/**
 * Implements the S88 bus protocol for reporting the state of tracks.
 * S88 is basically a long shift register where each bit corresponds
 * to a single contact on the track. Flip-flops on each S88 board make
 * sure activations are stored, so it is not necessary to query a
 * contact at the exact time it is activated. This implementation
 * allows a maximum of 512 bits or 32 full-width (16 bit) S88 boards.
 * The S88 standard recommends a maximum of 30 boards, so we should be
 * on the safe side.
 */

class TrackReporterS88
{

private:
  /**
   * The number of contacts available.
   */
  int mSize;
  /**
   * Stores the debug flag. When debugging is on, all outgoing and
   * incoming messages are printed to the Serial console.
   */
  bool mS88Debug;

  bool mS88IntEna;

  /**
   * The most recent contact values we know.
   */
  byte mSwitches[64];

public:
  /**
   * Creates a new TrackReporter with the given number of modules
   * being attached. While this value can be safely set to the
   * maximum of 32, it makes sense to specify the actual number,
   * since this speeds up reporting. The method assumes 16 bit
   * modules. If you use 8 bit modules instead (or both) you need
   * to do the math yourself.
   */
  TrackReporterS88(int modules);

  /**
   * Reads the current state of all contacts into the TrackReporter
   * and clears the flip-flops on all S88 boards. Call this method
   * periodically to have up-to-date values.
   */
  void refresh();
  void refresh(int inMaxSize);

  /**
   * Returns the state of an individual contact. Valid index values
   * are 1 to 512.
   */
  bool getValue(int index);

  /**
   * Returns the state of contacts. Valid index values
   * are 0 to 63.
   */
  byte getByte(int index);

  // void RepIntEna();

  // void RepIntDis();
};

#endif // TRACKREPORTER_H