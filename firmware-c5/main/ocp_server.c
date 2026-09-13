/*
 * ocp_server.c — reads command lines, dispatches, emits replies.
 *
 * SPDX-License-Identifier: MIT
 */

#include "ocp_server.h"
#include "ocp_frame.h"
#include "ocp_transport.h"
#include "radio_arbiter.h"
#include "status_led.h"
#include "wifi_inspect.h"
#include "wifi_recon.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_app_desc.h"
#include "esp_idf_version.h"
#include "esp_system.h"
#include "esp_timer.h"

#include "ocp.h"

#include <string.h>

#define OCP_TASK_STACK      4096
#define OCP_TASK_PRIO       10      /* above any engine task (DESIGN §6.4) */
#define OCP_READ_TIMEOUT_MS 100


#define X(id, verb, cc, mn, mx, reply) OCP_VID_##id,
typedef enum { OCP_VERB_TABLE(X) OCP_VID_COUNT } ocp_verb_id_t;
#undef X

#define X(id, verb, cc, mn, mx, reply) { verb, cc, mn, mx },
static const struct {
    const char      *verb;
    ocp_cap_class_t  cap;
    int              min_args;
    int              max_args;
} k_verbs[] = { OCP_VERB_TABLE(X) };
#undef X

/* Advertise only engines that actually came up: caps are a promise (§4). */
const char *ocp_server_caps(void)
{
    return wifi_recon_ready() ? OCP_CAP_WIFI24 OCP_CAP_SEP OCP_CAP_WIFI5 : "";
}

static void emit_hello(void)
{
    ocp_emit_compact(OCP_MARK_HELLO, "%s=%d %s=%s %s=%s %s=%s",
                     OCP_K_PROTO, OCP_PROTO_VERSION,
                     OCP_K_FW, OCP_FW_NAME_PROBE,
                     OCP_K_VER, esp_app_get_description()->version,
                     OCP_K_CAPS, ocp_server_caps());
}

/* A capability class is satisfied only if the build advertises a cap in it.
 * With no caps, every radio verb answers `nocap` rather than silently doing
 * nothing (OCP-SPEC §4). */
static int cap_available(ocp_cap_class_t cc)
{
    const char *caps = ocp_server_caps();
    switch (cc) {
        case OCP_CC_NONE:       return 1;
        case OCP_CC_WIFI:       return strstr(caps, OCP_CAP_WIFI24) != NULL ||
                                       strstr(caps, OCP_CAP_WIFI5) != NULL;
        case OCP_CC_BLE:        return strstr(caps, OCP_CAP_BLE) != NULL;
        case OCP_CC_IEEE802154: return strstr(caps, OCP_CAP_IEEE802154) != NULL;
        case OCP_CC_LORA_RX:    return strstr(caps, OCP_CAP_LORA_RX) != NULL;
    }
    return 0;
}

static void handle(ocp_verb_id_t id, int argc, char **argv)
{
    switch (id) {
    case OCP_VID_HELLO:
        emit_hello();
        break;

    case OCP_VID_PING:
        ocp_emit_literal(OCP_REPLY_PONG);
        break;

    case OCP_VID_VERSION: {
        const esp_app_desc_t *app = esp_app_get_description();
        ocp_emit_compact(OCP_MARK_VER, "%s=%s %s=%s idf=%s built=%s",
                         OCP_K_FW, OCP_FW_NAME_PROBE, OCP_K_VER, app->version,
                         IDF_VER, app->date);
        break;
    }

    case OCP_VID_STATUS:
        ocp_emit_compact(OCP_MARK_STATUS,
                         "%s=%s lora=absent link=%s %s=%llu heap=%u",
                         OCP_K_OWNER, arbiter_owner_name(arbiter_owner()), ocp_transport_name(),
                         OCP_K_UPTIME_MS,
                         (unsigned long long)(esp_timer_get_time() / 1000),
                         (unsigned)esp_get_free_heap_size());
        break;

    case OCP_VID_STOP: {
        /* Always acked, even when idle. A cancelled owner emits its own
         * aborted frame during teardown, so [STOP] comes last (§2.1). */
        bool running = arbiter_stop_all();
        ocp_emit_compact(OCP_MARK_STOP, "%s=%d", OCP_K_RUNNING, running ? 1 : 0);
        break;
    }

    case OCP_VID_SCAN_NETWORKS:
        wifi_cmd_scan();
        break;

    case OCP_VID_SHOW_SCAN_RESULTS:
        wifi_cmd_show_results(argc, argv);
        break;

    case OCP_VID_INSPECT_NETWORK:
        wifi_cmd_inspect(argc, argv);
        break;

    case OCP_VID_REBOOT:
        vTaskDelay(pdMS_TO_TICKS(50));   /* let the ack drain */
        esp_restart();
        break;

    default:
        /* In the contract and its cap is present, but no handler yet. */
        ocp_emit_error(OCP_ERR_UNKNOWN, "not implemented in this build");
        break;
    }
}

/* Split on whitespace, keeping a quoted run intact (OCP-SPEC §2). Returns the
 * token count, or -1 if a quote is left open. */
static int tokenize(char *line, char *argv[], int max_argv)
{
    int argc = 0;
    char *p = line;

    while (*p) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        if (argc >= max_argv) return -1;

        argv[argc++] = p;
        if (*p == '"') {
            p++;
            while (*p && *p != '"') {
                if (*p == '\\' && p[1]) p++;
                p++;
            }
            if (!*p) return -1;
            p++;
        }
        while (*p && *p != ' ' && *p != '\t') p++;
        if (*p) *p++ = '\0';
    }
    return argc;
}

static void dispatch(char *line)
{
    char *argv[OCP_MAX_ARGV];
    int argc = tokenize(line, argv, OCP_MAX_ARGV);

    if (argc < 0) {
        ocp_emit_error(OCP_ERR_BADARG, "malformed or too many arguments");
        return;
    }
    if (argc == 0) {
        return;   /* blank line is not an error */
    }

    for (size_t i = 0; i < sizeof k_verbs / sizeof k_verbs[0]; i++) {
        if (strcmp(argv[0], k_verbs[i].verb) != 0) continue;

        int nargs = argc - 1;
        if (nargs < k_verbs[i].min_args || nargs > k_verbs[i].max_args) {
            ocp_emit_error(OCP_ERR_BADARG, "wrong number of arguments");
            return;
        }
        if (!cap_available(k_verbs[i].cap)) {
            ocp_emit_error(OCP_ERR_NOCAP, "capability not present in this build");
            return;
        }
        status_led_activity();
        handle((ocp_verb_id_t)i, argc, argv);
        return;
    }

    ocp_emit_error(OCP_ERR_UNKNOWN, "unknown verb");
}

/* Assemble lines from the byte stream. An over-long line is reported once and
 * discarded to the next newline, so it cannot desynchronise what follows. */
static void ocp_task(void *arg)
{
    (void)arg;
    static char line[OCP_MAX_LINE_LEN + 1];
    size_t len = 0;
    bool overlong = false;
    uint8_t chunk[128];

    emit_hello();
    status_led_ready();

    for (;;) {
        status_led_kick();
        int n = ocp_transport_read(chunk, sizeof chunk, OCP_READ_TIMEOUT_MS);
        if (n <= 0) {
            /* A transport that returns immediately when idle would otherwise
             * spin this task at a priority above everything else and starve
             * the single core (DESIGN §6.4). Never trust the read to block. */
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        for (int i = 0; i < n; i++) {
            char c = (char)chunk[i];

            if (c == '\n' || c == '\r') {
                if (overlong) {
                    ocp_emit_error(OCP_ERR_BADARG, "line too long");
                } else if (len > 0) {
                    line[len] = '\0';
                    dispatch(line);
                }
                len = 0;
                overlong = false;
                continue;
            }
            if (overlong) continue;
            if (len >= OCP_MAX_LINE_LEN) {
                overlong = true;
                continue;
            }
            line[len++] = c;
        }
    }
}

esp_err_t ocp_server_start(void)
{
    BaseType_t ok = xTaskCreate(ocp_task, "ocp", OCP_TASK_STACK, NULL,
                                OCP_TASK_PRIO, NULL);
    return ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
