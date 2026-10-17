#pragma once

#include <functional>
#include <utility>

#include "platform/windows/connectivity_collector.h"

namespace sysmon::tests
{

/**
 * A connectivity subscription that calls onCancel when it is destroyed, as a
 * real subscription cancels its OS registration. A test's subscriber returns
 * one, and onCancel records the cancellation in the test's own state, under
 * that state's lock if other threads read it.
 */
class FakeSubscription : public platform::ConnectivitySubscription
{
public:
    explicit FakeSubscription(std::function<void()> onCancel) : m_onCancel(std::move(onCancel)) {}

    ~FakeSubscription() override
    {
        m_onCancel();
    }

    FakeSubscription(const FakeSubscription&) = delete;
    FakeSubscription& operator=(const FakeSubscription&) = delete;
    FakeSubscription(FakeSubscription&&) = delete;
    FakeSubscription& operator=(FakeSubscription&&) = delete;

private:
    std::function<void()> m_onCancel;
};

} // namespace sysmon::tests
