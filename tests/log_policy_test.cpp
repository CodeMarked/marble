#include "core/Log.hpp"

#include <cassert>
#include <cstdarg>
#include <cstring>
#include <string>

namespace {

std::string g_captured;

int callVlog(int verbosity, unsigned channels, char const* fmt, ...) {
    std::va_list ap;
    va_start(ap, fmt);
    const int r = marble::core::vlogPrintf(verbosity, channels, fmt, ap);
    va_end(ap);
    return r;
}

void captureSink(void* user, char const* text) {
    auto* out = static_cast<std::string*>(user);
    if (out != nullptr && text != nullptr) {
        if (!out->empty()) {
            *out += '|';
        }
        *out += text;
    }
}

} // namespace

int main() {
    using marble::core::LogChannel;
    using marble::core::allLogChannels;
    using marble::core::globalLogChannelFilter;
    using marble::core::globalLogVerbosity;
    using marble::core::logPrintf;
    using marble::core::setGlobalLogChannelFilter;
    using marble::core::setGlobalLogVerbosity;
    using marble::core::setLogSink;
    setLogSink(nullptr, nullptr);
    setGlobalLogVerbosity(0);
    setGlobalLogChannelFilter(allLogChannels());
    assert(globalLogVerbosity() == 0);
    assert(globalLogChannelFilter() == allLogChannels());

    g_captured.clear();
    setLogSink(captureSink, &g_captured);
    setGlobalLogVerbosity(2);

    const int rLow = logPrintf(1, static_cast<unsigned>(LogChannel::Engine), "a");
    assert(rLow > 0);
    assert(std::strstr(g_captured.c_str(), "a") != nullptr);

    g_captured.clear();
    const int rHigh = logPrintf(5, static_cast<unsigned>(LogChannel::Engine), "skip");
    assert(rHigh == 0);
    assert(g_captured.empty());

    setGlobalLogVerbosity(0);
    setGlobalLogChannelFilter(static_cast<unsigned>(LogChannel::Engine));
    g_captured.clear();
    (void)logPrintf(0, static_cast<unsigned>(LogChannel::General), "nochan");
    assert(g_captured.empty());

    g_captured.clear();
    (void)logPrintf(0, 0u, "untagged");
    assert(std::strstr(g_captured.c_str(), "untagged") != nullptr);

    g_captured.clear();
    (void)logPrintf(0, static_cast<unsigned>(LogChannel::Engine), "yes");
    assert(std::strstr(g_captured.c_str(), "yes") != nullptr);

    g_captured.clear();
    const int vr = callVlog(0, static_cast<unsigned>(LogChannel::Engine), "n=%d", 7);
    assert(vr > 0);
    assert(std::strstr(g_captured.c_str(), "n=7") != nullptr);

    setLogSink(nullptr, nullptr);
    setGlobalLogVerbosity(0);
    setGlobalLogChannelFilter(allLogChannels());
    return 0;
}
