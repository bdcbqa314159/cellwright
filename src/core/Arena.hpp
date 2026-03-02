#pragma once
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace magic {

class Arena {
public:
    explicit Arena(size_t initial_block = 4096);
    ~Arena();

    Arena(const Arena&) = delete;
    Arena& operator=(const Arena&) = delete;
    Arena(Arena&&) noexcept;
    Arena& operator=(Arena&&) noexcept;

    void* allocate(size_t size, size_t align = alignof(std::max_align_t));

    template <typename T, typename... Args>
    T* create(Args&&... args) {
        void* mem = allocate(sizeof(T), alignof(T));
        T* obj = new (mem) T(std::forward<Args>(args)...);
        if constexpr (!std::is_trivially_destructible_v<T>) {
            dtors_.push_back({&destroy<T>, obj});
        }
        return obj;
    }

    void reset();
    size_t bytes_used() const;

private:
    struct Block {
        uint8_t* data;
        size_t capacity;
    };

    struct DtorEntry {
        void (*fn)(void*);
        void* obj;
    };

    template <typename T>
    static void destroy(void* p) { static_cast<T*>(p)->~T(); }

    void grow(size_t min_size);

    std::vector<Block> blocks_;
    size_t current_offset_ = 0;
    size_t initial_block_size_;
    std::vector<DtorEntry> dtors_;
};

}  // namespace magic
