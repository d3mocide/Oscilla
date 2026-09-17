/*
 * wardrive_kml.h — KML formatting for the wardrive drive track + AP
 * placemarks (DESIGN §9.2: "KML — drive track + POIs per AP/device
 * (crash-tolerant, flushed incrementally, colored by security/type)."). Pure
 * formatting, no SD/file I/O — same split as wardrive_csv.h. Host-tested in
 * test/host/wardrive_kml_test.cpp.
 *
 * KML structure verified against Google's own KML reference (fetched
 * 2026-09-14): coordinate order is lon,lat[,alt] (easy to get backwards),
 * and <color> byte order is aabbggrr (alpha,blue,green,red — reversed from
 * the usual web rrggbb), confirmed rather than assumed from memory.
 *
 * "Crash-tolerant, flushed incrementally" (DESIGN's words) is handled the
 * way most incremental XML/KML loggers do it: the header opens the
 * <Document> and the drive-track's <LineString><coordinates> and
 * deliberately does *not* close them. Each kmlTrackPoint() append is a
 * self-contained coordinate line inside that still-open element. The logger
 * closes and reopens the track around each AP placemark so the assembled
 * document remains valid while both streams continue. If the session ends
 * without kmlFooter() (power loss, crash), a later boot can append the fixed
 * closing suffix without rewriting any completed observation.
 *
 * Raw SSID bytes are attacker-controlled (AGENTS.md "decoded bytes stay
 * hostile"). XML 1.0 forbids most C0 controls and does not accept arbitrary
 * bytes as UTF-8, so those bytes use reversible ASCII \xHH escapes rather
 * than reaching the document raw.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <string>

#include "model/scan_model.h"

namespace storage {

/* Opens the document: XML prolog, <kml>, <Document>, a fixed set of named
 * <Style> buckets for AP security tiers plus the track's line style, then
 * opens (but does not close) the track's <Placemark><LineString>
 * <coordinates>. Written once, at session start. */
std::string kmlHeader(const std::string &session_name);

/* Opens one track segment. The logger uses this after an AP placemark so
 * subsequent GNSS points remain outside that AP's Placemark. */
std::string kmlTrackStart();

/* One drive-track vertex, appended inside the still-open <coordinates> the
 * header opened. Safe to call any number of times, flushed after each. */
std::string kmlTrackPoint(double lat, double lon, double alt_m);

/* Closes the track's <coordinates></LineString></Placemark>. Call once,
 * after the last kmlTrackPoint() and before any kmlApPlacemark() — not
 * required for well-formedness (a Placemark nested inside another isn't
 * fatal to most readers) but keeps the file's structure honest. */
std::string kmlCloseTrack();

/* One AP observation as a styled, self-contained Placemark. The logger closes
 * its current track before appending this and opens the next segment after. */
std::string kmlApPlacemark(const model::ApRow &ap, double lat, double lon);

/* Closes </Document></kml>. Must be the last thing written for the file to
 * be strictly valid *as written*; see the file header comment on repairing
 * a session that ended before this was called. */
std::string kmlFooter();

/* Returns the suffix needed when the tail of an interrupted file ends at a
 * completed fragment. A complete footer needs no repair; a completed AP or
 * closed track needs only the document footer; any open track needs its
 * close sequence first. */
std::string kmlRecoverySuffix(const std::string &tail);

/* Host-testable recovery model: discard an incomplete final fragment from a
 * prefix and append the appropriate fixed closing suffix. Device startup
 * uses the bounded tail form above because it cannot load an entire session
 * into RAM. */
std::string kmlRecoverPrefix(const std::string &prefix);

}  // namespace storage
