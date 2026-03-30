#include "core/LockOrdering.hpp"

static_assert(marble::core::mayAcquireAfter(marble::core::LockLevel::Diagnostics,
                                            marble::core::LockLevel::ResourceAccess));
static_assert(marble::core::mayAcquireAfter(marble::core::LockLevel::ResourceAccess,
                                            marble::core::LockLevel::SimulationState));
static_assert(!marble::core::mayAcquireAfter(marble::core::LockLevel::SimulationState,
                                             marble::core::LockLevel::Diagnostics));
static_assert(!marble::core::mayAcquireAfter(marble::core::LockLevel::ResourceAccess,
                                             marble::core::LockLevel::Diagnostics));

int main() {
    return 0;
}
