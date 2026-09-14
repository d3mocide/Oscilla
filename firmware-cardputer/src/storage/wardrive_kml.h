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
 * self-contained coordinate line inside that still-open element; each
 * kmlApPlacemark() is a fully self-closing element safe to append any time.
 * If the session ends without kmlFooter() (power loss, crash), the file is
 * a well-formed *prefix* of a valid document, missing only its closing
 * tags — appending kmlFooter()'s fixed text later repairs it in place, no
 * rewrite of anything already written. That repair step is future work for
 * whatever opens a session's KML file next; this header only produces the
 * pieces.
 *
 * Raw SSID bytes are attacker-controlled (AGENTS.md "decoded bytes stay
 * hostile") and, unlike the CSV writer's octal-munge scheme, this is XML:
 * the escaper here also drops the C0 control bytes XML 1.0 forbids
 * outright (not even a numeric character reference rescues them — the
 * standard excludes them from documents entirely), rather than emitting
 * text that would make the file invalid.
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

/* One drive-track vertex, appended inside the still-open <coordinates> the
 * header opened. Safe to call any number of times, flushed after each. */
std::string kmlTrackPoint(double lat, double lon, double alt_m);

/* Closes the track's <coordinates></LineString></Placemark>. Call once,
 * after the last kmlTrackPoint() and before any kmlApPlacemark() — not
 * required for well-formedness (a Placemark nested inside another isn't
 * fatal to most readers) but keeps the file's structure honest. */
std::string kmlCloseTrack();

/* One AP observation as a styled, self-contained Placemark — safe to
 * append at any point after kmlHeader(), independent of the track's own
 * open/closed state. */
std::string kmlApPlacemark(const model::ApRow &ap, double lat, double lon);

/* Closes </Document></kml>. Must be the last thing written for the file to
 * be strictly valid *as written*; see the file header comment on repairing
 * a session that ended before this was called. */
std::string kmlFooter();

}  // namespace storage
