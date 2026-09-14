#pragma once

#include <atomic>
#include <cstdint>

#include "aion/gameserver/runtime/base/YieldPoint.h"

/**
 * Protocol mutations for the lifetime core's mutation tests (design §12.4: "prove the protocol").
 *
 * Every atomic step of the reference count / epoch / scan protocol that the safety argument relies on can be switched off (or replaced by
 * the broken variant) at runtime in test builds. tests/runtime/lifetime/ProtocolMutationTest.cpp runs each directed interleaving twice: with
 * Mutation::NONE it must pass, with the mutation it must detect a violation (use after free, double destruction, leak). A step whose removal
 * no test detects is either not needed for safety (documented at the mutation) or lacks a test.
 *
 * AION_LIFETIME_MUTATIONS defaults to AION_PCT (checked builds). In other builds isMutated() is a constant false and the switches cost nothing.
 * With mutations compiled in, each switch point costs one relaxed atomic load.
 */
#if !defined(AION_LIFETIME_MUTATIONS)
#define AION_LIFETIME_MUTATIONS AION_PCT
#endif

namespace aion::gameserver::runtime::detail {

enum class Mutation : uint32_t {
	NONE = 0,
	// ------------------------------------------------------------------------------------------------------------------- RefCounted::release
	/** the last release (1 -> 0) does not stamp retireEpoch */
	SKIP_RELEASE_STAMP,
	/** releases CAS the count first and stamp afterwards (stamp after the object may already be destroyed) */
	CAS_BEFORE_STAMP,
	/** the design's literal protocol: releases with count > 1 skip the stamp (unsafe under count ABA, see RefCounted.h) */
	DESIGN_STAMPLESS_FAST_PATH,
	/** the stamp is stored unconditionally instead of max(retireEpoch, E) (a slow stamper can lower it) */
	STAMP_OVERWRITE,
	/** the count is decremented with load + store instead of a CAS (lost decrements) */
	NON_ATOMIC_DECREMENT,
	/** the last release pushes without the `queued` exchange (double queueing) */
	RELEASE_ALWAYS_PUSH,
	// ------------------------------------------------------------------------------------------------------------------- Reclaimer scan
	/** the scan does not advance E */
	SCAN_NO_ADVANCE,
	/** m ignores published epochs */
	SCAN_IGNORE_PUBLISHED,
	/** m is computed after the objects' count/retireEpoch were read */
	SCAN_MIN_AFTER_OBJECTS,
	/** an object found with count > 0 is dropped from the queue without clearing `queued` */
	SCAN_KEEP_WITHOUT_CLEAR,
	/** after clearing `queued`, the count is not re-checked (a concurrent last release is lost) */
	SCAN_DROP_WITHOUT_RECHECK,
	/** after clearing `queued`, a re-checked zero count keeps the object without the `queued` exchange (double queueing) */
	SCAN_REQUEUE_WITHOUT_EXCHANGE,
	/** retirePart stamps epoch 0 */
	PART_STAMP_ZERO,
	/** retireNode stamps epoch 0 */
	NODE_STAMP_ZERO,
	/** the scan destroys a retired OwnedPart by its stamp alone, ignoring Refs to the part (the design's literal §2.3 rule) */
	PART_REFS_IGNORED,
	/** OwnedPart::release does not stamp the part (a borrow taken from a Ref<Part> after retirement is unprotected) */
	PART_RELEASE_NO_STAMP,
	// ------------------------------------------------------------------------------------------------------------------- TaskScope
	/** ensurePublished() does nothing */
	PUBLISH_NOTHING,
	/**
	 * ensurePublished() stores E once without the re-check loop. NOT a safety step of this protocol (the scan's min is compared against stamps
	 * read after unlinks, and a publication missed by a scan implies a load after that scan's advance); kept because the design specifies it.
	 * ProtocolMutationTest documents this with an interleaving that stays safe under the mutation.
	 */
	PUBLISH_NO_RECHECK,
	/** quiescentPoint() unpublishes regardless of QuiescentScope, depth and JOIN helper state */
	QUIESCENT_IGNORE_RULES,
	/** leaving the outermost TaskScope does not unpublish */
	SCOPE_EXIT_NO_UNPUBLISH,
	/** creating a Ptr from a Ref, T& or T* does not publish (the design's literal §2.4: only pointer loads publish) */
	BORROW_NO_PUBLISH,
	// ------------------------------------------------------------------------------------------------------------------- Reclaimer liveness
	/**
	 * NOT a safety step: the scan examines every limbo bucket instead of only the keys below m, like the scan before the limbo (every scan
	 * re-walks the whole backlog, also the part a pinned epoch keeps alive). ReclaimerLivenessTest detects it by the examined-entry counts.
	 */
	SCAN_EXAMINE_WHOLE_LIMBO,
};

#if AION_LIFETIME_MUTATIONS

extern std::atomic<Mutation> activeMutation;

inline bool isMutated(Mutation mutation) noexcept {
	return activeMutation.load(std::memory_order_relaxed) == mutation;
}

#else

constexpr bool isMutated(Mutation) noexcept {
	return false;
}

#endif

/** RAII: activates a mutation for the lifetime of the object (tests only; not thread-safe against concurrent MutationScopes). */
class MutationScope {
public:
	explicit MutationScope([[maybe_unused]] Mutation mutation) noexcept {
#if AION_LIFETIME_MUTATIONS
		previous = activeMutation.exchange(mutation);
#endif
	}
	~MutationScope() {
#if AION_LIFETIME_MUTATIONS
		activeMutation.store(previous);
#endif
	}
	MutationScope(const MutationScope&) = delete;
	MutationScope& operator=(const MutationScope&) = delete;

private:
	[[maybe_unused]] Mutation previous = Mutation::NONE;
};

#if AION_LIFETIME_MUTATIONS
namespace testing {
/** entries classified per scan chunk (0 = the default, 64; values above 64 are clamped) */
extern std::atomic<uint32_t> scanChunkOverride;
/** entry budget of reclaimNow() and therefore drain() (0 = unbounded, the default) */
extern std::atomic<uint64_t> reclaimNowEntryBudget;
} // namespace testing
#endif

/**
 * RAII, tests only (review fix: the PCT and mutation scenarios retire far fewer than one chunk and scan without a budget, so they never produced
 * multi-chunk or budget-split scans): scans classify `chunk` entries before destroying them and reclaimNow() examines at most `reclaimNowEntries`
 * entries per scan (0 = unbounded), so destructors, cascades and budget cuts interleave with the classification of later entries. A no-op
 * without AION_LIFETIME_MUTATIONS. Not thread-safe against concurrent ScanShapeScopes.
 */
class ScanShapeScope {
public:
	ScanShapeScope([[maybe_unused]] uint32_t chunk, [[maybe_unused]] uint64_t reclaimNowEntries) noexcept {
#if AION_LIFETIME_MUTATIONS
		previousChunk = testing::scanChunkOverride.exchange(chunk);
		previousEntries = testing::reclaimNowEntryBudget.exchange(reclaimNowEntries);
#endif
	}
	~ScanShapeScope() {
#if AION_LIFETIME_MUTATIONS
		testing::scanChunkOverride.store(previousChunk);
		testing::reclaimNowEntryBudget.store(previousEntries);
#endif
	}
	ScanShapeScope(const ScanShapeScope&) = delete;
	ScanShapeScope& operator=(const ScanShapeScope&) = delete;

private:
	[[maybe_unused]] uint32_t previousChunk = 0;
	[[maybe_unused]] uint64_t previousEntries = 0;
};

} // namespace aion::gameserver::runtime::detail
