#pragma once

#include "../common/config.h"
#include <cstdint>
#include <list>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace latticedb {

enum class AccessType { Unknown, Lookup, Scan, Index };

class Replacer {
public:
  virtual ~Replacer() = default;

  virtual bool victim(frame_id_t* frame_id) = 0;

  virtual void pin(frame_id_t frame_id) = 0;

  virtual void unpin(frame_id_t frame_id) = 0;

  virtual void record_access(frame_id_t frame_id, AccessType access_type) = 0;

  virtual void set_evictable(frame_id_t frame_id, bool set_evictable) = 0;

  virtual void remove(frame_id_t frame_id) = 0;

  virtual size_t size() = 0;
};

class LRUReplacer : public Replacer {
private:
  struct LRUNode {
    frame_id_t frame_id;
    bool is_evictable;
    std::list<frame_id_t>::iterator list_iter;

    explicit LRUNode(frame_id_t fid) : frame_id(fid), is_evictable(true) {}
  };

  std::list<frame_id_t> lru_list_;
  std::unordered_map<frame_id_t, std::unique_ptr<LRUNode>> lru_hash_;
  std::mutex latch_;
  size_t max_size_;

public:
  explicit LRUReplacer(size_t num_pages);

  ~LRUReplacer() override = default;

  bool victim(frame_id_t* frame_id) override;

  void pin(frame_id_t frame_id) override;

  void unpin(frame_id_t frame_id) override;

  void record_access(frame_id_t frame_id, AccessType access_type) override;

  void set_evictable(frame_id_t frame_id, bool set_evictable) override;

  void remove(frame_id_t frame_id) override;

  size_t size() override;

private:
  void move_to_head(frame_id_t frame_id);

  void add_to_head(frame_id_t frame_id);

  void remove_from_list(frame_id_t frame_id);
};

class ClockReplacer : public Replacer {
private:
  struct ClockFrame {
    frame_id_t frame_id;
    bool reference_bit;
    bool is_evictable;

    explicit ClockFrame(frame_id_t fid = -1)
        : frame_id(fid), reference_bit(false), is_evictable(true) {}
  };

  std::vector<ClockFrame> clock_;
  std::unordered_map<frame_id_t, size_t> frame_to_idx_;
  size_t clock_hand_;
  size_t capacity_;
  size_t size_;
  std::mutex latch_;

public:
  explicit ClockReplacer(size_t num_pages);

  ~ClockReplacer() override = default;

  bool victim(frame_id_t* frame_id) override;

  void pin(frame_id_t frame_id) override;

  void unpin(frame_id_t frame_id) override;

  void record_access(frame_id_t frame_id, AccessType access_type) override;

  void set_evictable(frame_id_t frame_id, bool set_evictable) override;

  void remove(frame_id_t frame_id) override;

  size_t size() override;

private:
  size_t advance_hand();

  void remove_from_clock(size_t idx);

  size_t find_empty_slot();
};

} // namespace latticedb
