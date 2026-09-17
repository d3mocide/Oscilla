/*
 * wardrive_logger.h — writes a wardrive session to the deck's microSD
 * (DESIGN §9.2): a WigleWifi-1.6 CSV of AP observations and a KML drive
 * track + placemarks, side by side. Row/document shapes live in
 * wardrive_csv.h and wardrive_kml.h; this file only owns the file lifecycle,
 * same split as lora_logger.h.
 *
 * AP rows are written **only while the fix is valid**. A WigleWifi row with
 * no position isn't a weaker row, it's a wrong one — real consumers treat
 * lat/lon as authoritative. Observations seen without a fix are counted
 * (`aps_no_fix`) rather than written, and the KML track simply has a gap for
 * that period. That gap plus the two counters are how a session shows
 * "fix" and "no fix" distinctly, which is P4's exit gate.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>

#include "model/gnss_model.h"
#include "model/scan_model.h"

namespace storage {

struct WardriveLogStats {
    bool open = false;
    uint32_t aps = 0;           /* observations written with a valid fix    */
    uint32_t aps_no_fix = 0;    /* observations dropped for want of a fix   */
    uint32_t track_points = 0;  /* drive-track vertices                     */
};

/* Opens /oscilla/wardrive/drive_NNNN.{csv,kml}. Numbered, not date-named:
 * a session can legitimately start before the first fix, so the filename
 * cannot depend on one. The UTC date goes *inside* both files instead, when
 * it is known. False if SD isn't ready or no unused index was found. After a
 * marked card fault, one explicit remount attempt is made; there is no
 * background retry loop. */
bool wardriveLogBegin(const model::GnssFix &fix);

/* One drive-track vertex. Call on a valid fix; no-op otherwise. */
void wardriveLogTrackPoint(const model::GnssFix &fix);

/* One AP observation. Counted but not written unless `fix.valid`. */
void wardriveLogAp(const model::ApRow &ap, const model::GnssFix &fix, uint32_t fix_age_ms);

/* Closes the track and the KML document, then both files. */
void wardriveLogEnd();

const WardriveLogStats &wardriveLogStats();

/* "drive_0001", or "" when no session is open. */
const char *wardriveLogName();

}  // namespace storage
