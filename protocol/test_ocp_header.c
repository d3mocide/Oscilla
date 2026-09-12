/*
 * test_ocp_header.c — contract test for ocp.h. Run via tools/check_protocol.sh.
 *
 * SPDX-License-Identifier: MIT
 */
#include "ocp.h"
#include "ocp.h"   /* the include guard must make this a no-op */

#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Each expansion of the verb table must compile. */
#define X(id, verb, cc, mn, mx, reply) OCP_VID_##id,
typedef enum { OCP_VERB_TABLE(X) OCP_VERB_COUNT } ocp_verb_id_t;
#undef X

#define X(id, verb, cc, mn, mx, reply) { verb, cc, mn, mx, reply },
static const struct {
    const char      *verb;
    ocp_cap_class_t  cap;
    int              min_args;
    int              max_args;
    const char      *reply;
} k_verbs[] = { OCP_VERB_TABLE(X) };
#undef X

/* A transmit-shaped verb name fails the D-8 guarantee. */
static const char *k_tx_needles[] = {
    "_tx", "tx_", "transmit", "send", "inject", "deauth_attack",
    "beacon_spam", "jam", "evil_twin", "rogue"
};

int main(void)
{
    const size_t n = sizeof(k_verbs) / sizeof(k_verbs[0]);
    assert(n == (size_t)OCP_VERB_COUNT);
    assert(n > 0);

    for (size_t i = 0; i < n; i++) {
        /* Well-formed row. */
        assert(k_verbs[i].verb != NULL && k_verbs[i].verb[0] != '\0');
        assert(strlen(k_verbs[i].verb) < OCP_MAX_VERB_LEN);
        assert(k_verbs[i].min_args >= 0);
        assert(k_verbs[i].max_args >= k_verbs[i].min_args);
        assert(k_verbs[i].max_args < OCP_MAX_ARGV);  /* verb + args must fit argv */

        /* A duplicate would shadow silently at dispatch. */
        for (size_t j = i + 1; j < n; j++) {
            assert(strcmp(k_verbs[i].verb, k_verbs[j].verb) != 0);
        }

        /* D-8: no transmit verb in any build. */
        for (size_t k = 0; k < sizeof(k_tx_needles) / sizeof(k_tx_needles[0]); k++) {
            if (strstr(k_verbs[i].verb, k_tx_needles[k]) != NULL) {
                fprintf(stderr,
                        "FAIL: verb '%s' matches transmit pattern '%s' "
                        "(DESIGN.md §8, D-8)\n",
                        k_verbs[i].verb, k_tx_needles[k]);
                return 1;
            }
        }
    }

    /* No capability advertises transmit. */
    const char *caps[] = { OCP_CAP_WIFI24, OCP_CAP_WIFI5, OCP_CAP_BLE,
                           OCP_CAP_IEEE802154, OCP_CAP_LORA_RX };
    for (size_t i = 0; i < sizeof(caps) / sizeof(caps[0]); i++) {
        assert(strstr(caps[i], "tx") == NULL || strcmp(caps[i], OCP_CAP_LORA_RX) == 0);
        assert(strchr(caps[i], ',') == NULL);   /* caps= is comma-separated */
        assert(strchr(caps[i], ' ') == NULL);
    }

    /* Bracketed and whitespace-free, so a parser can key on the first token. */
    const char *marks[] = { OCP_MARK_HELLO, OCP_MARK_VER, OCP_MARK_STATUS,
                            OCP_MARK_STOP, OCP_MARK_CFG, OCP_MARK_SCAN,
                            OCP_MARK_INSPECT, OCP_MARK_SNIFF, OCP_MARK_CLIENTS,
                            OCP_MARK_PROBES, OCP_MARK_CHAN, OCP_MARK_BLE,
                            OCP_MARK_ZIG, OCP_MARK_LORA, OCP_MARK_EVT,
                            OCP_MARK_ERR, OCP_MARK_FT };
    for (size_t i = 0; i < sizeof(marks) / sizeof(marks[0]); i++) {
        size_t len = strlen(marks[i]);
        assert(len >= 3 && marks[i][0] == '[' && marks[i][len - 1] == ']');
        assert(strchr(marks[i], ' ') == NULL);
        for (size_t j = i + 1; j < sizeof(marks) / sizeof(marks[0]); j++) {
            assert(strcmp(marks[i], marks[j]) != 0);
        }
    }

    /* Protocol identity and transport constants. */
    assert(OCP_PROTO_VERSION == 1);
    assert(strcmp(OCP_PROTO_VERSION_STR, "1") == 0);
    assert(OCP_BAUD_DEFAULT == 115200);
    assert(OCP_MAX_LINE_LEN >= 256);
    assert(strcmp(ocp_cap_class_name(OCP_CC_NONE), "") == 0);
    assert(strcmp(ocp_cap_class_name(OCP_CC_LORA_RX), OCP_CAP_LORA_RX) == 0);

    printf("ocp.h OK — %zu verbs, proto=%d, no transmit verb\n", n, OCP_PROTO_VERSION);
    return 0;
}
