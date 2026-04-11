#pragma once

#include "gameplay/GameTransport.hpp"

#include <cstddef>
#include <cstdint>

namespace marble::gameplay::test {

/// Wraps an [`IGameTransport`] for tests: probabilistically drops outbound datagrams on `send`.
/// `dropOneInN == 0` disables drops. Uses a simple LCG for reproducibility.
class ImpairedGameTransport final : public IGameTransport {
public:
    ImpairedGameTransport(IGameTransport* inner, std::uint32_t dropOneInN, std::uint32_t seed) noexcept
        : inner_(inner)
        , dropOneInN_(dropOneInN)
        , rng_(seed == 0u ? 0xC0FFEEu : seed) {}

    [[nodiscard]] bool send(PeerId to, void const* data, std::size_t len) noexcept override {
        if (inner_ == nullptr) {
            return false;
        }
        if (dropOneInN_ > 0u) {
            std::uint32_t const x = rng_;
            rng_ = x * 1664525u + 1013904223u;
            if ((rng_ % dropOneInN_) == 0u) {
                ++dropsObserved_;
                return true;
            }
        }
        return inner_->send(to, data, len);
    }

    [[nodiscard]] std::size_t receive(PeerId& outFrom, void* buffer, std::size_t bufferBytes) noexcept override {
        if (inner_ == nullptr) {
            return 0u;
        }
        return inner_->receive(outFrom, buffer, bufferBytes);
    }

    void forgetPeer(PeerId peer) noexcept override {
        if (inner_ != nullptr) {
            inner_->forgetPeer(peer);
        }
    }

    [[nodiscard]] std::uint32_t dropsObserved() const noexcept { return dropsObserved_; }

private:
    IGameTransport* inner_{};
    std::uint32_t dropOneInN_{};
    std::uint32_t rng_{};
    std::uint32_t dropsObserved_{};
};

} // namespace marble::gameplay::test
