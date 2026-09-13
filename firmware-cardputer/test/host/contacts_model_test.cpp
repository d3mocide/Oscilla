/*
 * contacts_model_test.cpp — host test for model::ContactsModel.
 *
 * SPDX-License-Identifier: MIT
 */

#include <cstdio>
#include <string>

#include "model/contacts_model.h"
#include "ocp/ocp_parser.h"

namespace {

int g_pass, g_fail;
void check(bool ok, const char *what)
{
    std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", what);
    (ok ? g_pass : g_fail)++;
}

ocp::Item frameOf(const std::string &wire)
{
    ocp::Parser p;
    ocp::Item out;
    p.feed(reinterpret_cast<const uint8_t *>(wire.data()), wire.size(),
           [&](ocp::Item &&it) { if (it.kind == ocp::ItemKind::Frame) out = std::move(it); });
    return out;
}

ocp::Item eventOf(const std::string &wire)
{
    ocp::Parser p;
    ocp::Item out;
    p.feed(reinterpret_cast<const uint8_t *>(wire.data()), wire.size(),
           [&](ocp::Item &&it) { if (it.kind == ocp::ItemKind::Event) out = std::move(it); });
    return out;
}

}  // namespace

int main()
{
    {
        model::ContactsModel m;
        m.begin();
        check(m.sniffing() && m.clients().empty() && m.probes().empty(), "begin() starts clean");

        std::string w = "[CLIENTS] BEGIN n=2 total=2 elapsed_ms=1500\n"
                        "[CLIENTS] \"aa:bb:cc:dd:ee:01\",\"f4:12:34:56:78:9a\",\"1\",\"2.4\",\"-54\",\"128\"\n"
                        "[CLIENTS] \"aa:bb:cc:dd:ee:02\",\"10:22:33:44:55:66\",\"36\",\"5\",\"-70\",\"4\"\n"
                        "[CLIENTS] END\n";
        m.absorbClients(frameOf(w));
        check(m.clients().size() == 2 && m.malformedRows() == 0, "two client rows parsed");
        check(m.clients()[0].bssid == "aa:bb:cc:dd:ee:01" && m.clients()[0].mac == "f4:12:34:56:78:9a",
              "bssid/mac fields");
        check(m.clients()[0].ch == 1 && !m.clients()[0].band5 && m.clients()[0].rssi == -54 &&
              m.clients()[0].pkts == 128, "ch/band/rssi/pkts");
        check(m.clients()[1].band5 && m.clients()[1].ch == 36, "5 GHz row");
    }
    {
        model::ContactsModel m;
        m.begin();
        std::string w = "[PROBES] BEGIN n=2 total=2 elapsed_ms=1500\n"
                        "[PROBES] \"f4:12:34:56:78:9a\",\"HomeNet\",\"-61\",\"3\"\n"
                        "[PROBES] \"f4:12:34:56:78:9a\",\"\",\"-58\",\"7\"\n"
                        "[PROBES] END\n";
        m.absorbProbes(frameOf(w));
        check(m.probes().size() == 2, "two probe rows parsed");
        check(m.probes()[0].ssid == "HomeNet" && m.probes()[0].rssi == -61 && m.probes()[0].pkts == 3,
              "directed probe fields");
        check(m.probes()[1].ssid.empty(), "wildcard probe: empty SSID, not dropped");
    }
    {
        model::ContactsModel m;
        m.begin();
        std::string w = "[CLIENTS] BEGIN n=3 total=3\n"
                        "[CLIENTS] \"aa:bb:cc:dd:ee:01\",\"f4:12:34:56:78:9a\",\"1\",\"2.4\",\"-54\",\"128\"\n"
                        "[CLIENTS] \"aa:bb\",\"f4:12:34:56:78:9a\",\"1\",\"2.4\",\"-54\",\"1\"\n"       /* short bssid */
                        "[CLIENTS] \"aa:bb:cc:dd:ee:03\",\"f4:12:34:56:78:9a\",\"999\",\"2.4\",\"-54\",\"1\"\n" /* bad ch */
                        "[CLIENTS] END\n";
        m.absorbClients(frameOf(w));
        check(m.clients().size() == 1 && m.malformedRows() == 2, "malformed rows counted and skipped, not trusted");
    }
    {
        model::ContactsModel m;
        m.begin();
        m.absorbEvent(eventOf("[EVT] kind=sniff pkts=118 ch=6\n"));
        check(m.totalPkts() == 118 && m.currentChannel() == 6, "sniff event updates ticker counters");

        m.absorbEvent(eventOf(
            "[EVT] kind=client bssid=aa:bb:cc:dd:ee:01 mac=f4:12:34:56:78:9a ch=6 rssi=-54\n"));
        check(m.lastSighting().find("f4:12:34:56:78:9a") != std::string::npos, "client event updates the ticker text");
        check(m.clients().empty(), "an event never mutates the authoritative table, only the ticker");

        m.absorbEvent(eventOf("[EVT] kind=probe mac=f4:12:34:56:78:9a ssid=\"HomeNet\" rssi=-61\n"));
        check(m.lastSighting().find("HomeNet") != std::string::npos, "probe event updates the ticker text");
    }
    {
        model::ContactsModel m;
        m.begin();
        m.absorbEvent(eventOf("[EVT] kind=sniff pkts=42 ch=1\n"));
        std::string w = "[CLIENTS] BEGIN n=1 total=1\n"
                        "[CLIENTS] \"aa:bb:cc:dd:ee:01\",\"f4:12:34:56:78:9a\",\"1\",\"2.4\",\"-54\",\"1\"\n"
                        "[CLIENTS] END\n";
        m.absorbClients(frameOf(w));
        m.stopSniffing();
        check(!m.sniffing() && m.clients().size() == 1, "stop clears live state but keeps the table");

        m.clear();
        check(!m.sniffing() && m.clients().empty() && m.totalPkts() == 0, "probe reset clears everything");
    }

    std::printf("\n%s: %d passed, %d failed\n", g_fail ? "contacts model test FAILED" : "contacts model test OK",
               g_pass, g_fail);
    return g_fail ? 1 : 0;
}
