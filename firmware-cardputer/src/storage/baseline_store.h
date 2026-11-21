/*
 * baseline_store.h — SD persistence for /oscilla/baselines/default.csv (WP-11).
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "model/baseline_watch.h"

namespace storage {

bool saveBaseline(const model::BaselineWatch &watch);
bool loadBaseline(model::BaselineWatch &watch);
bool clearSavedBaseline();

}  // namespace storage
