# oscilla-c5 — the probe. ESP-IDF project.
#
# Requires an ESP-IDF release with ESP32-C5 support; verify the version you
# install actually targets C5 before P1 (ROADMAP P1 entry gate).
#   idf.py set-target esp32c5 && idf.py build
cmake_minimum_required(VERSION 3.16)

# The OCP contract lives outside this project and is shared with the deck.
# Both firmwares compile against the same header so the contract cannot drift.
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/components")

# Reported by `version` and [HELLO]; read from the repo-root VERSION file
# (docs/RELEASING.md) so IDF does not derive it from git.
file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/../VERSION" PROJECT_VER LIMIT_COUNT 1)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/../VERSION")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(oscilla-c5)
