#pragma once

#include <cassert>
#include <cstddef>
#include <span>
#include <vector>

namespace sysmon::monitoring
{

/**
 * A fixed-capacity circular buffer for time-series metric samples.
 *
 * New samples are pushed to the back. When the buffer is full,
 * the oldest sample is overwritten. The buffer is not thread-safe;
 * the caller must synchronize access.
 *
 * Internally, storage is pre-allocated at 2x capacity with mirrored writes,
 * allowing samples() to return a contiguous span in chronological order in O(1)
 * without any dynamic memory allocations on push.
 *
 * @tparam T The sample type. Must be default-constructible and copyable.
 */
template <typename T> class RingBuffer
{
public:
    /**
     * Constructs a ring buffer with the given capacity.
     * @param capacity Maximum number of samples. Must be > 0.
     */
    explicit RingBuffer(std::size_t capacity) : m_capacity(capacity)
    {
        assert(capacity > 0 && "RingBuffer capacity must be greater than zero");
        m_storage.resize(capacity * 2);
    }

    /**
     * Pushes a sample to the back of the buffer.
     * If the buffer is full, the oldest sample is overwritten.
     */
    void push(const T &sample)
    {
        const std::size_t pos = m_head;
        m_storage[pos] = sample;
        m_storage[pos + m_capacity] = sample;
        m_head = (m_head + 1) % m_capacity;
        if (m_size < m_capacity) {
            m_size++;
        }
    }

    /**
     * Pushes a sample to the back of the buffer using move semantics.
     * If the buffer is full, the oldest sample is overwritten.
     */
    void push(T &&sample)
    {
        const std::size_t pos = m_head;
        // Copy into primary slot, then move into mirror. Both slots must hold
        // the value for the contiguous-span trick, so we cannot avoid one copy.
        m_storage[pos] = sample;
        m_storage[pos + m_capacity] = std::move(sample);
        m_head = (m_head + 1) % m_capacity;
        if (m_size < m_capacity) {
            m_size++;
        }
    }

    /** Returns the number of samples currently stored. */
    [[nodiscard]] std::size_t size() const
    {
        return m_size;
    }

    /** Returns the maximum capacity of the buffer. */
    [[nodiscard]] std::size_t capacity() const
    {
        return m_capacity;
    }

    /** Returns true if no samples are currently stored. */
    [[nodiscard]] bool empty() const
    {
        return m_size == 0;
    }

    /** Returns true if the buffer has reached maximum capacity. */
    [[nodiscard]] bool full() const
    {
        return m_size == m_capacity;
    }

    /** Clears all samples from the buffer without reallocating storage. */
    void clear()
    {
        m_head = 0;
        m_size = 0;
    }

    /** Returns a span over the stored samples in chronological order (oldest to newest). */
    [[nodiscard]] std::span<const T> samples() const
    {
        if (m_size == 0) {
            return {};
        }
        const std::size_t start = (m_size < m_capacity) ? 0 : m_head;
        return std::span<const T>(m_storage.data() + start, m_size);
    }

    /** Returns the sample at the specified chronological index (0 = oldest). */
    [[nodiscard]] const T &operator[](std::size_t index) const
    {
        assert(index < m_size && "Index out of bounds");
        return samples()[index];
    }

    /** Returns the oldest sample in the buffer. */
    [[nodiscard]] const T &front() const
    {
        assert(m_size > 0 && "RingBuffer is empty");
        return samples().front();
    }

    /** Returns the newest sample in the buffer. */
    [[nodiscard]] const T &back() const
    {
        assert(m_size > 0 && "RingBuffer is empty");
        return samples().back();
    }

    [[nodiscard]] auto begin() const
    {
        return samples().begin();
    }
    [[nodiscard]] auto end() const
    {
        return samples().end();
    }
    [[nodiscard]] auto cbegin() const
    {
        return samples().cbegin();
    }
    [[nodiscard]] auto cend() const
    {
        return samples().cend();
    }

private:
    std::size_t m_capacity{0};
    std::size_t m_size{0};
    std::size_t m_head{0};
    std::vector<T> m_storage;
};

} // namespace sysmon::monitoring
