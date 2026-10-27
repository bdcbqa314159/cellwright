#include "core/Arena.hpp"
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <new>
#include <new>

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
      retired_(o.retired_),
      initial_block_size_(o.initial_block_size_),
      dtors_(std::move(o.dtors_)) {
    o.current_offset_ = 0;
    o.retired_ = 0;
}

Arena& Arena::operator=(Arena&& o) noexcept {
    if (this != &o) {
        reset();
        for (auto& b : blocks_) std::free(b.data);

        blocks_ = std::move(o.blocks_);
        current_offset_ = o.current_offset_;
        retired_ = o.retired_;
        initial_block_size_ = o.initial_block_size_;
        dtors_ = std::move(o.dtors_);
        o.current_offset_ = 0;
        o.retired_ = 0;
    }
    return *this;
}

void* Arena::allocate(size_t size, size_t align) {
    assert(!blocks_.empty());
    assert(align > 0 && (align & (align - 1)) == 0 && "align must be a power of two");

    // Align the ABSOLUTE address, never the offset. std::malloc only guarantees
    // alignof(std::max_align_t) (16 on this target), so a block's start address is
    // not necessarily aligned to a larger requested alignment — and when it is not,
    // an aligned offset does not yield an aligned pointer.
    const auto aligned_addr = [align](const Block& b, size_t off) -> uintptr_t {
        const uintptr_t raw = reinterpret_cast<uintptr_t>(b.data) + off;
        return (raw + align - 1) & ~static_cast<uintptr_t>(align - 1);
    };

    uintptr_t addr = aligned_addr(blocks_.back(), current_offset_);
    size_t pad = addr - (reinterpret_cast<uintptr_t>(blocks_.back().data) + current_offset_);

    if (current_offset_ + pad + size > blocks_.back().capacity) {
        grow(size + align - 1);  // size plus worst-case alignment padding
        addr = aligned_addr(blocks_.back(), 0);
        pad = addr - reinterpret_cast<uintptr_t>(blocks_.back().data);
    }

    // Padding is consumed space and is counted, not lost.
    current_offset_ += pad + size;
    return reinterpret_cast<void*>(addr);
}

void Arena::reset() {
    // Call destructors in reverse registration order
    for (auto it = dtors_.rbegin(); it != dtors_.rend(); ++it) {
        it->fn(it->obj);
    }
    dtors_.clear();
    current_offset_ = 0;
    retired_ = 0;
}

size_t Arena::bytes_used() const {
    // Actually-consumed bytes. Summing non-last block CAPACITIES (the previous
    // approach) over-reports: a block abandoned by growth is not full, and after
    // reset() nothing is used at all.
    return retired_ + current_offset_;
}

void Arena::grow(size_t min_size) {
    size_t cap = blocks_.empty()
        ? initial_block_size_
        : blocks_.back().capacity * 2;
    if (cap < min_size) cap = min_size;

    auto* data = static_cast<uint8_t*>(std::malloc(cap));
    if (!data) throw std::bad_alloc();

    // Record what the outgoing block consumed before current_offset_ is reset,
    // or bytes_used() loses it. Skipped on the first grow (no outgoing block).
    if (!blocks_.empty()) retired_ += current_offset_;

    blocks_.push_back({data, cap});
    current_offset_ = 0;
}

}  // namespace magic
