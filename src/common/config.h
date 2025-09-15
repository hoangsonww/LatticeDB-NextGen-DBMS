#pragma once

#include <cstdint>
#include <string>

namespace latticedb {

static constexpr uint32_t PAGE_SIZE = 4096;
static constexpr uint32_t BUFFER_POOL_SIZE = 1024;
static constexpr uint32_t LOG_BUFFER_SIZE = 1024 * 1024;  // 1MB
static constexpr uint32_t HASH_TABLE_BUCKET_SIZE = 1024;
static constexpr uint32_t HASH_TABLE_DIRECTORY_SIZE = 256;

static constexpr uint32_t INVALID_PAGE_ID = UINT32_MAX;
static constexpr uint32_t INVALID_TXN_ID = UINT32_MAX;
static constexpr uint32_t INVALID_LSN = UINT32_MAX;
static constexpr uint32_t HEADER_PAGE_ID = 0;

static constexpr uint32_t LOG_TIMEOUT = 1000;  // milliseconds

static constexpr char DEFAULT_DB_FILE[] = "latticedb.db";
static constexpr char DEFAULT_LOG_FILE[] = "latticedb.log";

enum class IsolationLevel {
    READ_UNCOMMITTED,
    READ_COMMITTED,
    REPEATABLE_READ,
    SERIALIZABLE
};

enum class LockMode {
    SHARED,
    EXCLUSIVE,
    INTENTION_SHARED,
    INTENTION_EXCLUSIVE,
    SHARED_INTENTION_EXCLUSIVE
};

enum class AbortReason {
    LOCK_TIMEOUT,
    DEADLOCK,
    USER_ABORT,
    SERIALIZATION_FAILURE
};

}