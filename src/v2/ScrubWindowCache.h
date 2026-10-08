#pragma once
#include <array>
#include <atomic>
#include <cassert>

namespace ndaw::v2
{
// Two stable slots. Only the non-real-time producer constructs/reclaims data.
// A callback borrows a slot with at most two CAS attempts; no shared_ptr release,
// allocation, lock, waiting or retry loop occurs on the callback.
template <class Window> class ScrubWindowCache
{
    struct Slot
    {
        Window data;
        std::atomic<int> users{0}; // -1: producer owns it; >=0: callback borrowers
    };
    static_assert(std::atomic<int>::is_always_lock_free);

public:
    class Lease
    {
    public:
        Lease() = default;
        Lease(const Lease&) = delete;
        Lease& operator=(const Lease&) = delete;
        ~Lease()
        {
            if (slot)
                slot->users.fetch_sub(1, std::memory_order_release);
        }
        const Window* operator->() const noexcept
        {
            return &slot->data;
        }
        explicit operator bool() const noexcept
        {
            return slot != nullptr;
        }

    private:
        friend class ScrubWindowCache;
        explicit Lease(Slot* s) : slot(s) {}
        Slot* slot = nullptr;
    };

    Lease read() noexcept
    {
        for (int attempt = 0; attempt < 2; ++attempt)
        {
            const int index = current.load(std::memory_order_acquire);
            if (index < 0)
                return {};
            auto& slot = slots[index];
            int count = slot.users.load(std::memory_order_relaxed);
            if (count >= 0 && count < 128 &&
                slot.users.compare_exchange_strong(count, count + 1, std::memory_order_acquire))
                return Lease(&slot);
        }
        return {};
    }
    // Single producer/message thread. A stale callback may have loaded the old
    // index before publication; CAS either protects its borrow or refuses it.
    bool claimForWrite(int index) noexcept
    {
        assert(index >= 0 && index < 2);
        int expected = 0;
        return index != frontIndex() && slots[index].users.compare_exchange_strong(expected, -1);
    }
    void publish(int index) noexcept
    {
        assert(slots[index].users.load() == -1);
        slots[index].users.store(0, std::memory_order_release);
        current.store(index, std::memory_order_release);
    }
    void abandon(int index) noexcept
    {
        assert(slots[index].users.load() == -1);
        slots[index].users.store(0, std::memory_order_release);
    }
    int frontIndex() const noexcept
    {
        return current.load(std::memory_order_acquire);
    }
    Window& data(int index) noexcept
    {
        return slots[index].data;
    }
    const Window& data(int index) const noexcept
    {
        return slots[index].data;
    }

private:
    std::array<Slot, 2> slots;
    std::atomic<int> current{-1};
};
} // namespace ndaw::v2
