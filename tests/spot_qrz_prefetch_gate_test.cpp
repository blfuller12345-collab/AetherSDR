// Rate gate for the quiet QRZ lookups behind spot distance/heading (#6603):
// one request per 10 s, at most 30 in any rolling hour (QRZ blocks an IP
// that sends too many).
#include "core/SpotQrzPrefetcher.h"

#include <cstdio>

using AetherSDR::SpotQrzPrefetcher;

namespace {
int g_failed = 0;
void expectTrue(const char* name, bool ok)
{
    std::printf("[%s] %s\n", ok ? " OK " : "FAIL", name);
    if (!ok)
        ++g_failed;
}
} // namespace

int main()
{
    SpotQrzPrefetcher::Gate g;
    const qint64 t = 1'000'000;
    expectTrue("first request allowed", g.allow(t));
    g.record(t);
    expectTrue("second request too soon", !g.allow(t + 5'000));
    expectTrue("allowed after the interval", g.allow(t + SpotQrzPrefetcher::kMinIntervalMs));

    SpotQrzPrefetcher::Gate h;
    qint64 now = 0;
    int sent = 0;
    for (int i = 0; i < 200; ++i) {  // 2000 s of back-to-back demand
        now += SpotQrzPrefetcher::kMinIntervalMs;
        if (h.allow(now)) {
            h.record(now);
            ++sent;
        }
    }
    expectTrue("hourly cap holds", sent == SpotQrzPrefetcher::kMaxPerHour);
    expectTrue("allowed again an hour after the first",
               h.allow(h.sentMs.first() + 3600LL * 1000));

    if (g_failed == 0) {
        std::printf("\nAll spot QRZ prefetch gate tests passed.\n");
        return 0;
    }
    std::printf("\n%d spot QRZ prefetch gate test(s) failed.\n", g_failed);
    return 1;
}
