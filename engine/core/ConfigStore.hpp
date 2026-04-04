#pragma once

#include "core/ClosedHashTable.hpp"
#include "core/StringId.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstdlib>
#include <string>
#include <string_view>

namespace marble::core {

struct ConfigOption {
    StringId id{};
    std::string key{};
    std::string value{};
    bool persistent{true};
    bool perUser{false};
};

template <std::size_t Capacity = 256>
class ConfigStore {
public:
    [[nodiscard]] bool set(std::string_view key, std::string value, bool persistent = true, bool perUser = false) {
        if (key.empty()) {
            return false;
        }
        const StringId id = makeStringId(key);
        ConfigOption opt{id, std::string(key), std::move(value), persistent, perUser};
        return options_.insertOrAssign(id, std::move(opt));
    }

    [[nodiscard]] ConfigOption const* find(std::string_view key) const {
        const ConfigOption* p = options_.find(makeStringId(key));
        if (p == nullptr || p->key != key) {
            return nullptr;
        }
        return p;
    }

    [[nodiscard]] bool getString(std::string_view key, std::string& out) const {
        const ConfigOption* p = find(key);
        if (p == nullptr) {
            return false;
        }
        out = p->value;
        return true;
    }

    [[nodiscard]] bool getInt(std::string_view key, int& out) const {
        const ConfigOption* p = find(key);
        if (p == nullptr) {
            return false;
        }
        int v{};
        const char* first = p->value.data();
        const char* last = first + p->value.size();
        const auto [ptr, ec] = std::from_chars(first, last, v);
        if (ec != std::errc{} || ptr != last) {
            return false;
        }
        out = v;
        return true;
    }

    [[nodiscard]] bool getFloat(std::string_view key, float& out) const {
        const ConfigOption* p = find(key);
        if (p == nullptr) {
            return false;
        }
        char* end = nullptr;
        const float v = std::strtof(p->value.c_str(), &end);
        if (end == p->value.c_str() || *end != '\0') {
            return false;
        }
        out = v;
        return true;
    }

    [[nodiscard]] bool getBool(std::string_view key, bool& out) const {
        const ConfigOption* p = find(key);
        if (p == nullptr) {
            return false;
        }
        std::string lowered = p->value;
        std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        if (lowered == "1" || lowered == "true" || lowered == "yes" || lowered == "on") {
            out = true;
            return true;
        }
        if (lowered == "0" || lowered == "false" || lowered == "no" || lowered == "off") {
            out = false;
            return true;
        }
        return false;
    }

    void loadKeyValueText(std::string_view text, bool persistent = true, bool perUser = false) {
        std::size_t start = 0;
        while (start <= text.size()) {
            std::size_t end = text.find('\n', start);
            if (end == std::string_view::npos) {
                end = text.size();
            }
            std::string_view line = trim(text.substr(start, end - start));
            if (!line.empty() && line.front() != '#' && line.front() != ';' &&
                !(line.size() >= 2 && line[0] == '/' && line[1] == '/') &&
                !(line.front() == '[' && line.back() == ']')) {
                const std::size_t eq = line.find('=');
                if (eq != std::string_view::npos) {
                    const std::string_view key = trim(line.substr(0, eq));
                    const std::string_view value = trim(line.substr(eq + 1));
                    if (!key.empty()) {
                        (void)set(key, std::string(value), persistent, perUser);
                    }
                }
            }
            start = end + 1;
        }
    }

    void applyCommandLineArg(std::string_view arg, bool persistent = false, bool perUser = true) {
        if (!arg.starts_with("--")) {
            return;
        }
        arg.remove_prefix(2);
        const std::size_t eq = arg.find('=');
        if (eq == std::string_view::npos) {
            return;
        }
        const std::string_view key = trim(arg.substr(0, eq));
        const std::string_view value = trim(arg.substr(eq + 1));
        if (!key.empty()) {
            (void)set(key, std::string(value), persistent, perUser);
        }
    }

    void applyCommandLine(int argc, const char* const* argv, bool persistent = false, bool perUser = true) {
        for (int i = 1; i < argc; ++i) {
            applyCommandLineArg(argv[i], persistent, perUser);
        }
    }

private:
    static std::string_view trim(std::string_view v) {
        while (!v.empty() && std::isspace(static_cast<unsigned char>(v.front())) != 0) {
            v.remove_prefix(1);
        }
        while (!v.empty() && std::isspace(static_cast<unsigned char>(v.back())) != 0) {
            v.remove_suffix(1);
        }
        return v;
    }

    ClosedHashTable<StringId, ConfigOption, Capacity, StringIdHash> options_{};
};

} // namespace marble::core
