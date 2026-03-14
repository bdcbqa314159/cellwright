#include "core/Arena.hpp"
#include <cassert>
#include <cstdlib>
#include <cstring>

namespace magic {

Arena::Arena(size_t initial_block)
    : initial_block_size_(initial_block) {
    grow(initial_block);
}

Arena::~Arena() {
    reset();
    for (auto& b : blocks_) {
        std::free(b.data);
    }
}

Arena::Arena(Arena&& o) noexcept
    : blocks_(std::move(o.blocks_)),
      current_offset_(o.current_offset_),
      initial_block_size_(o.initial_block_size_),
      dtors_(std::move(o.dtors_)) {
    o.current_offset_ = 0;
}

Arena& Arena::operator=(Arena&& o) noexcept {
    if (this != &o) {
        reset();
        for (auto& b : blocks_) std::free(b.data);

        blocks_ = std::move(o.blocks_);
        current_offset_ = o.current_offset_;
        initial_block_size_ = o.initial_block_size_;
        dtors_ = std::move(o.dtors_);
        o.current_offset_ = 0;
    }
    return *this;
}

void* Arena::allocate(size_t size, size_t align) {
    assert(!blocks_.empty());
    auto& blk = blocks_.back();

    // Align current_offset_ up
    size_t aligned = (current_offset_ + align - 1) & ~(align - 1);
    if (aligned + size > blk.capacity) {
        // Allocate enough for the requested size plus worst-case alignment padding
        grow(size + align - 1);
        auto& new_blk = blocks_.back();
        // current_offset_ is 0 after grow(); re-align from 0
        size_t new_aligned = (0 + align - 1) & ~(align - 1);  // 0 for power-of-2 align
        current_offset_ = new_aligned + size;
        return new_blk.data + new_aligned;
    }
    current_offset_ = aligned + size;
    return blk.data + aligned;
}

void Arena::reset() {
    // Call destructors in reverse registration order
    for (auto it = dtors_.rbegin(); it != dtors_.rend(); ++it) {
        it->fn(it->obj);
    }
    dtors_.clear();
    current_offset_ = 0;
}

size_t Arena::bytes_used() const {
    size_t total = 0;
    if (blocks_.empty()) return 0;
    // All blocks except the last are fully used
    for (size_t i = 0; i + 1 < blocks_.size(); ++i) {
        total += blocks_[i].capacity;
    }
    total += current_offset_;
    return total;
}

void Arena::grow(size_t min_size) {
    size_t cap = blocks_.empty()
        ? initial_block_size_
        : blocks_.back().capacity * 2;
    if (cap < min_size) cap = min_size;

    auto* data = static_cast<uint8_t*>(std::malloc(cap));
    if (!data) throw std::bad_alloc();
    blocks_.push_back({data, cap});
    current_offset_ = 0;
}

}  // namespace magic
