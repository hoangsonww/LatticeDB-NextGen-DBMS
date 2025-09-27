#include <mutex>

#include "replacer.h"

namespace latticedb {

// LRUReplacer implementation
LRUReplacer::LRUReplacer(size_t num_pages) : max_size_(num_pages) {}

bool LRUReplacer::victim(frame_id_t* frame_id) {
  std::lock_guard<std::mutex> l(latch_);
  for (auto it = lru_list_.rbegin(); it != lru_list_.rend(); ++it) {
    auto fid = *it;
    auto& node = lru_hash_[fid];
    if (node && node->is_evictable) {
      *frame_id = fid;
      remove(fid);
      return true;
    }
  }
  return false;
}

void LRUReplacer::pin(frame_id_t frame_id) {
  std::lock_guard<std::mutex> l(latch_);
  auto it = lru_hash_.find(frame_id);
  if (it != lru_hash_.end() && it->second) {
    it->second->is_evictable = false;
  }
}

void LRUReplacer::unpin(frame_id_t frame_id) {
  std::lock_guard<std::mutex> l(latch_);
  auto it = lru_hash_.find(frame_id);
  if (it == lru_hash_.end() || !it->second) {
    add_to_head(frame_id);
  } else {
    it->second->is_evictable = true;
    move_to_head(frame_id);
  }
}

void LRUReplacer::record_access(frame_id_t frame_id, AccessType) {
  unpin(frame_id);
}

void LRUReplacer::set_evictable(frame_id_t frame_id, bool set_evictable) {
  std::lock_guard<std::mutex> l(latch_);
  auto it = lru_hash_.find(frame_id);
  if (it != lru_hash_.end() && it->second)
    it->second->is_evictable = set_evictable;
}

void LRUReplacer::remove(frame_id_t frame_id) {
  std::lock_guard<std::mutex> l(latch_);
  remove_from_list(frame_id);
  lru_hash_.erase(frame_id);
}

size_t LRUReplacer::size() {
  std::lock_guard<std::mutex> l(latch_);
  return lru_hash_.size();
}

void LRUReplacer::move_to_head(frame_id_t frame_id) {
  auto it = lru_hash_.find(frame_id);
  if (it == lru_hash_.end())
    return;
  remove_from_list(frame_id);
  lru_list_.push_front(frame_id);
  it->second->list_iter = lru_list_.begin();
}

void LRUReplacer::add_to_head(frame_id_t frame_id) {
  auto node = std::make_unique<LRUNode>(frame_id);
  lru_list_.push_front(frame_id);
  node->list_iter = lru_list_.begin();
  lru_hash_[frame_id] = std::move(node);
  if (max_size_ != 0 && lru_list_.size() > max_size_) {
    auto tail = lru_list_.back();
    lru_list_.pop_back();
    lru_hash_.erase(tail);
  }
}

void LRUReplacer::remove_from_list(frame_id_t frame_id) {
  auto it = lru_hash_.find(frame_id);
  if (it == lru_hash_.end())
    return;
  lru_list_.erase(it->second->list_iter);
}

// ClockReplacer implementation (simple)
ClockReplacer::ClockReplacer(size_t num_pages) : clock_hand_(0), capacity_(num_pages), size_(0) {
  clock_.resize(num_pages);
}

bool ClockReplacer::victim(frame_id_t* frame_id) {
  std::lock_guard<std::mutex> l(latch_);
  if (size_ == 0)
    return false;
  size_t examined = 0;
  while (examined < capacity_) {
    auto& slot = clock_[clock_hand_];
    if (slot.is_evictable) {
      if (slot.reference_bit) {
        slot.reference_bit = false;
      } else if (slot.frame_id != static_cast<frame_id_t>(-1)) {
        *frame_id = slot.frame_id;
        frame_to_idx_.erase(slot.frame_id);
        slot.frame_id = static_cast<frame_id_t>(-1);
        --size_;
        advance_hand();
        return true;
      }
    }
    advance_hand();
    ++examined;
  }
  return false;
}

void ClockReplacer::pin(frame_id_t frame_id) {
  std::lock_guard<std::mutex> l(latch_);
  auto it = frame_to_idx_.find(frame_id);
  if (it != frame_to_idx_.end()) {
    auto& slot = clock_[it->second];
    slot.is_evictable = false;
    slot.reference_bit = true;
  }
}

void ClockReplacer::unpin(frame_id_t frame_id) {
  std::lock_guard<std::mutex> l(latch_);
  auto it = frame_to_idx_.find(frame_id);
  if (it == frame_to_idx_.end()) {
    // add new
    size_t idx = find_empty_slot();
    clock_[idx] = ClockFrame(frame_id);
    clock_[idx].is_evictable = true;
    clock_[idx].reference_bit = true;
    frame_to_idx_[frame_id] = idx;
    ++size_;
  } else {
    auto& slot = clock_[it->second];
    slot.is_evictable = true;
    slot.reference_bit = true;
  }
}

void ClockReplacer::record_access(frame_id_t frame_id, AccessType) {
  unpin(frame_id);
}

void ClockReplacer::set_evictable(frame_id_t frame_id, bool set_evictable) {
  std::lock_guard<std::mutex> l(latch_);
  auto it = frame_to_idx_.find(frame_id);
  if (it != frame_to_idx_.end())
    clock_[it->second].is_evictable = set_evictable;
}

void ClockReplacer::remove(frame_id_t frame_id) {
  std::lock_guard<std::mutex> l(latch_);
  auto it = frame_to_idx_.find(frame_id);
  if (it == frame_to_idx_.end())
    return;
  clock_[it->second].frame_id = static_cast<frame_id_t>(-1);
  frame_to_idx_.erase(it);
  if (size_ > 0)
    --size_;
}

size_t ClockReplacer::size() {
  std::lock_guard<std::mutex> l(latch_);
  return size_;
}

size_t ClockReplacer::advance_hand() {
  clock_hand_ = (clock_hand_ + 1) % capacity_;
  return clock_hand_;
}

void ClockReplacer::remove_from_clock(size_t idx) {
  (void)idx;
}

size_t ClockReplacer::find_empty_slot() {
  for (size_t i = 0; i < capacity_; ++i) {
    size_t idx = (clock_hand_ + i) % capacity_;
    if (clock_[idx].frame_id == static_cast<frame_id_t>(-1))
      return idx;
  }
  // fallback overwrite current
  return clock_hand_;
}

} // namespace latticedb
