#include "single_instance.h"

#include <algorithm>
#include <thread>
#include <utility>

#include <QString>

#include "instance_server.h"

namespace verdandi::app
{

namespace
{

// How long to wait before trying again while the running instance starts or closes.
constexpr std::chrono::milliseconds kRetryInterval{100};

} // namespace

InstanceClaim claimSingleInstance(std::string_view application, std::chrono::milliseconds timeout)
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    for (;;) {
        platform::InstanceMutex mutex(application);
        if (mutex.state() == platform::InstanceMutex::State::Created) {
            return {.result = InstanceClaimResult::Claimed, .mutex = std::move(mutex)};
        }
        if (mutex.state() == platform::InstanceMutex::State::Failed) {
            return {.result = InstanceClaimResult::Unchecked, .problem = mutex.problem()};
        }

        const auto remaining =
            std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());
        if (InstanceServer::askToBringToFront(QString::fromStdString(mutex.name()),
                                              std::max(remaining, std::chrono::milliseconds::zero()))) {
            return {.result = InstanceClaimResult::BroughtToFront};
        }
        // The instance is starting, before its server listens, closing, after
        // its server has stopped, or busy. Once it has closed, the next
        // attempt creates the mutex.
        if (std::chrono::steady_clock::now() >= deadline) {
            return {.result = InstanceClaimResult::NotAnswered};
        }
        std::this_thread::sleep_for(kRetryInterval);
    }
}

} // namespace verdandi::app
