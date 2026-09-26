#pragma once

#include <atomic>
#include <cstdint>

#include "aion/gameserver/runtime/lifetime/RefCounted.h"

// Internal to the lifetime core: access to RefCounted's protocol state for the Reclaimer and white-box tests. Not part of the public API.

namespace aion::gameserver::runtime::detail {

struct RefCountedAccess {
	static std::atomic<uint32_t>& count(const RefCounted& object) noexcept { return object.count; }
	static std::atomic<bool>& queued(const RefCounted& object) noexcept { return object.queued; }
	static std::atomic<uint64_t>& retireEpoch(const RefCounted& object) noexcept { return object.retireEpoch; }
#if AION_CHECKED
	static std::atomic<RefCounted::Cookie>& cookie(const RefCounted& object) noexcept { return object.cookie; }
#endif
	/** Runs the most derived destructor without freeing memory (the Reclaimer frees the allocation itself). */
	static void destroy(const RefCounted& object) noexcept { object.~RefCounted(); }
};

/**
 * Allocation of RefCounted objects (makeRef). Checked builds put a 16-byte header {size, magic} in front of every object so the Reclaimer
 * can poison the whole allocation and keep it in the delayed-free FIFO (C3); release builds allocate exactly sizeof(T).
 */
struct ObjectHeader {
	uint64_t size;
	uint64_t magic;
};
inline constexpr uint64_t OBJECT_HEADER_MAGIC = 0xA10B'1EC7'0B1E'C700;

} // namespace aion::gameserver::runtime::detail
