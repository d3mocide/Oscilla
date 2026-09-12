# oscilla-c5 — the probe. ESP-IDF project.
#
# Requires an ESP-IDF release with ESP32-C5 support; verify the version you
# install actually targets C5 before P1 (ROADMAP P1 entry gate).
#   idf.py set-target esp32c5 && idf.py build
cmake_minimum_required(VERSION 3.16)

# The OCP contract lives outside this project and is shared with the deck.
# Both firmwares compile against the same header so the contract cannot drift.
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/components")

# Reported by `version` and [HELLO]; without this IDF derives it from git.
set(PROJECT_VER "0.1.0")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(oscilla-c5)
