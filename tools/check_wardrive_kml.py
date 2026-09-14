#!/usr/bin/env python3
"""Validate that the assembled wardrive KML document is actually well-formed
XML, not just individually plausible-looking pieces.

test/host/wardrive_kml_test.cpp --sample prints one document assembled from
kmlHeader()+kmlTrackPoint()x2+kmlCloseTrack()+kmlApPlacemark()x2+kmlFooter().
String-equality unit tests on each piece can't catch a mismatched tag
between two of them the way a real parser can — this is that check, same
role tools/check_ocp_text.py plays for the C field encoder.

SPDX-License-Identifier: MIT
"""

import subprocess
import sys
import xml.etree.ElementTree as ET

NS = "{http://www.opengis.net/kml/2.2}"


def main(test_bin: str) -> int:
    doc = subprocess.run([test_bin, "--sample"], capture_output=True, text=True, check=True).stdout

    try:
        root = ET.fromstring(doc)
    except ET.ParseError as e:
        print(f"FAIL: assembled KML is not well-formed XML: {e}", file=sys.stderr)
        return 1

    if root.tag != f"{NS}kml":
        print(f"FAIL: root element is {root.tag!r}, not kml in the KML 2.2 namespace", file=sys.stderr)
        return 1

    placemarks = root.findall(f".//{NS}Placemark")
    if len(placemarks) != 3:
        print(f"FAIL: expected 3 Placemarks (1 track + 2 APs), found {len(placemarks)}", file=sys.stderr)
        return 1

    styles = {s.get("id") for s in root.findall(f".//{NS}Style")}
    expected_styles = {"sOpen", "sWep", "sSecure", "sEnterprise", "sOther", "sTrack"}
    if styles != expected_styles:
        print(f"FAIL: style ids {styles} != expected {expected_styles}", file=sys.stderr)
        return 1

    track_coords = root.find(f".//{NS}LineString/{NS}coordinates")
    if track_coords is None or len(track_coords.text.strip().splitlines()) != 2:
        print("FAIL: track LineString should have exactly 2 coordinate lines", file=sys.stderr)
        return 1

    print(f"  wardrive KML: well-formed, {len(placemarks)} placemarks, {len(styles)} styles")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1]))
