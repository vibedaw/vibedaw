#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <mutex>
#include <thread>

namespace vibedaw {

// Single producer and single consumer. Only the producer overwrites/reclaims T.
// The consumer's reference survives until its next acquire(), never beyond it.
template <typename T> class LatestState {
public:
    void publish(const T& value) {
        slots[back] = value;
        back = middle.exchange(back | dirty, std::memory_order_acq_rel) & mask;
    }
    const T& acquire() noexcept {
        if (middle.load(std::memory_order_acquire) & dirty)
            front = middle.exchange(front, std::memory_order_acq_rel) & mask;
        return slots[front];
    }
private:
    static constexpr unsigned dirty = 4, mask = 3;
    std::array<T, 3> slots{};
    unsigned back = 2, front = 0;
    std::atomic<unsigned> middle{1};
};

template <typename T, unsigned Capacity> class BoundedQueue {
    static_assert(Capacity > 0 && (Capacity & (Capacity - 1)) == 0,
                  "Power-of-two capacity preserves indexing across unsigned wrap");
public:
    bool push(const T& value) noexcept {
        auto w = write.load(std::memory_order_relaxed);
        if (w - read.load(std::memory_order_acquire) == Capacity) return false;
        data[w % Capacity] = value;
        write.store(w + 1, std::memory_order_release);
        return true;
    }
    bool pop(T& value) noexcept {
        auto r = read.load(std::memory_order_relaxed);
        if (r == write.load(std::memory_order_acquire)) return false;
        value = data[r % Capacity];
        read.store(r + 1, std::memory_order_release);
        return true;
    }
private:
    std::array<T, Capacity> data{};
    std::atomic<unsigned> write{0}, read{0};
};

// One engine, one render consumer. Writers serialize off audio. Sequentially
// consistent admission ensures a writer cannot miss an admitted render block.
class AudioQuiescence {
public:
    class Edit {
    public:
        Edit() : lock(instance().writers) {
            if (depth++ == 0) {
                instance().blocked.store(true);
                while (instance().active.load()) std::this_thread::yield();
            }
        }
        ~Edit() { if (--depth == 0) instance().blocked.store(false); }
    private:
        std::unique_lock<std::recursive_mutex> lock;
        inline static thread_local unsigned depth = 0;
    };
    static AudioQuiescence& instance() { return singleton; }
    bool enter() noexcept {
        active.store(true);
        if (!blocked.load()) return true;
        active.store(false);
        return false;
    }
    void leave() noexcept { active.store(false); }
private:
    static AudioQuiescence singleton;
    std::recursive_mutex writers;
    std::atomic<bool> blocked{false}, active{false};
};
inline AudioQuiescence AudioQuiescence::singleton;
static_assert(std::atomic<unsigned>::is_always_lock_free);
static_assert(std::atomic<bool>::is_always_lock_free);
static_assert(std::atomic<float>::is_always_lock_free);
static_assert(std::atomic<int>::is_always_lock_free);

} // namespace vibedaw
