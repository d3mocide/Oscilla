/* zig_recon.h — OCP and arbiter integration for passive 802.15.4 RX. */
#pragma once
#include <stdbool.h>
#include "esp_err.h"

esp_err_t zig_recon_init(void);
bool zig_recon_ready(void);
void zig_cmd_start(int argc, char **argv);
void zig_cmd_status(void);
void zig_cmd_list(void);
void zig_cmd_nodes(int argc, char **argv);
void zig_cmd_clear(void);
