#pragma once

#include <chrono>
#include <cstdint>
#include <initializer_list>
#include <span>
#include <string>

#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::utils::idfactory {

/**
 * Java: com.aionemu.gameserver.utils.idfactory.IDFactoryError (a java.lang.Error: not meant to be caught; the C++ port throws it and lets the
 * startup code or the task wrapper log it).
 */
class IDFactoryError : public runtime::Exception {
public:
	using runtime::Exception::Exception;
};

/**
 * Java: com.aionemu.gameserver.utils.idfactory.IDFactory - object id allocation (design §6, RR-10/RR-16, deviation 7).
 *
 * Differences from Java (by design):
 * - Monotone cursor: nextId() allocates the lowest free valid id at or above the cursor and moves the cursor behind it; releaseId() never
 *   moves the cursor down (Java moves it down to the released id, so the id would be handed out again at once). When no free id is left in
 *   [cursor, wrapAt) (gameserver.idfactory.wrap_at, default 2^27 = 16 MB of bits) the cursor wraps: the lowest free id >= 1 is taken. Only if
 *   every id below wrapAt is used are ids >= wrapAt allocated. Ids held as bare ints by handlers (SummonerAI, Npc.creatorId) are therefore not
 *   reused within an encounter.
 * - Release floor: a released id enters a FIFO and its bit stays set for at least `releaseDelay` (gameserver.idfactory.release_delay, 300 s)
 *   before it becomes free, also across a wrap. The FIFO is drained lazily by nextId()/releaseId() and by drainQuarantine(). releaseDelay 0
 *   frees immediately (the cursor still does not move down). Releasing an id that is taken but still in quarantine logs the Java "wasn't taken"
 *   warning (a double release).
 * - Checked builds: recentlyReleased(id) lets World.storeObject warn when an id released less than `reuseWarningWindow` (1 h) ago is reassigned
 *   (C16), naming the previous class (the class name passed to releaseId, "unknown" if none was given).
 * Unchanged from Java: the invalid-id bit pattern (INVALID_ID_BIT_MASK/INVALID_ID_BITCHECK) is never allocated; getInstance() locks id 0 and logs
 * "IDFactory: N IDs used."; lockIds at startup throws IDFactoryError for an id taken twice; releasing an id that is not taken logs a warning
 * with a stack trace; "All IDs are used" throws IDFactoryError.
 *
 * DAO seeding (Java initializeUsedIds): the startup code calls lockIds once per DAO (PlayerDAO, InventoryDAO, PlayerRegisteredItemsDAO,
 * LegionDAO, MailDAO, GuideDAO, HousesDAO, PlayerPetsDAO getUsedIDs()) before the first nextId(), then logUsedCount().
 *
 * Time comes from the kernel Clock of ThreadPoolManager's backend (steady clock; ManualClock in tests), read before the lock.
 * Locks: an IDFACTORY leaf mutex; nothing is logged and no callback runs under it. Callers must not hold a leaf mutex of rank >= IDFACTORY.
 * Memory: two bit sets of max(highest used id, wrapAt)/8 bytes (16 MB each at the default wrapAt, allocated as ids grow).
 * Thread-safety: all members are thread-safe.
 */
class IDFactory {
public:
	struct Config {
		/** gameserver.idfactory.wrap_at (>= 2) */
		int32_t wrapAt = 1 << 27;
		/** gameserver.idfactory.release_delay (>= 0) */
		std::chrono::seconds releaseDelay{300};
		/** checked builds: how long released ids are remembered for the reuse warning */
		std::chrono::seconds reuseWarningWindow{3600};
	};

	static constexpr int32_t INVALID_ID_BIT_MASK = 0b0010011111100110101111111111100;
	static constexpr int32_t INVALID_ID_BITCHECK = 0b0000000000000000001100101010100;

	/** The process-wide instance (Java SingletonHolder). The first call creates it with lockIds(0) and without DAO seeding. */
	static IDFactory& getInstance();

	/** Applies the configuration (before the server starts allocating). @throws IllegalArgumentException for wrapAt < 2 or a negative delay */
	void configure(const Config& config);
	Config getConfig() const;

	/**
	 * Java lockIds(int...) as used by initializeUsedIds(): marks ids loaded from the DAOs as used. Ids before the throwing one stay locked.
	 * @throws IDFactoryError if an id is already taken or negative
	 */
	void lockIds(std::span<const int32_t> ids);
	void lockIds(std::initializer_list<int32_t> ids) { lockIds(std::span<const int32_t>(ids.begin(), ids.size())); }

	/** Java constructor log line "IDFactory: {} IDs used." (after DAO seeding) */
	void logUsedCount() const;

	/** @throws IDFactoryError if there is no free id */
	int32_t nextId();

	/**
	 * Releases an id (enters the quarantine FIFO). `className` names the object kind for the checked-build reuse warning (static storage).
	 * Logs a warning with a stack trace if the id was not taken (Java).
	 */
	void releaseId(int32_t id, const char* className = nullptr);

	/** Java releaseObjectIds(Collection<AionObject>): releases each id. */
	void releaseObjectIds(std::span<const int32_t> ids, const char* className = nullptr);

	/** Java getUsedCount(): taken ids, including ids still in quarantine */
	int32_t getUsedCount() const;

	/** ids waiting in the release FIFO */
	int32_t getQuarantinedCount() const;

	/** Frees every quarantined id whose release delay has elapsed. @return number of ids freed */
	int32_t drainQuarantine();

	/** checked builds: the class name of the object that released `id` within the reuse warning window, else nullptr (release builds: nullptr) */
	const char* recentlyReleased(int32_t id) const;

	/** the allocation cursor (next search start), for tests and //debug */
	int64_t getCursor() const;

	static bool isInvalidId(int32_t id) noexcept { return (id & INVALID_ID_BIT_MASK) == INVALID_ID_BITCHECK; }

	/** Test support: forgets every allocation and quarantine entry, restores the default configuration and locks id 0 again. */
	void resetForTests();

private:
	IDFactory() = default;
};

} // namespace aion::gameserver::utils::idfactory
