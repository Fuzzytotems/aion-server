#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <typeinfo>

namespace aion::gameserver::runtime {

/**
 * Lock class of a game-level lock, the unit of lock-order validation (design §3.4 "Lock classes", RR-9, §4.2).
 *
 * - Member monitors, collection shims and Atomic* members carry a static class `DeclaringClass::field`, generated with the member:
 *   `ArrayList<Ref<Npc>> traps{AION_LOCK_CLASS(LowerUdasTempleInstance::traps)};`
 * - Object monitors (`SYNCHRONIZED(*this)`, `SYNCHRONIZED(npc)`) use the dynamic type of the object: LockClass::ofType(typeid(object)).
 * - ConcurrentHashMap stripe monitors use `Owner::field#stripe`.
 * - Shims constructed without a class fall back to a class named after the shim type (e.g. "ArrayList"), which is legal but makes lockdep
 *   reports coarser.
 *
 * Instances are interned and immortal: the same name always yields the same object, `name()` stays valid forever, so per-thread records and
 * the watchdog can keep `name().c_str()` pointers (RR-18). Thread-safe.
 */
class LockClass {
public:
	LockClass(const LockClass&) = delete;
	LockClass& operator=(const LockClass&) = delete;

	/** The interned class with this name (created on first use). Thread-safe; takes an internal plain mutex (no callbacks, never a Monitor). */
	static const LockClass& named(std::string_view name);

	/**
	 * The interned class of a C++ type, named by commons getClassName (e.g. "aion::gameserver::model::gameobjects::Npc"). Cached per type in a
	 * lock-free table: after the first call for a type this is a hash, a few atomic loads and no lock (object monitors call it on every
	 * SYNCHRONIZED).
	 */
	static const LockClass& ofType(const std::type_info& type);

	/** Fallback class for anonymous Monitors ("Monitor"). */
	static const LockClass& anonymousMonitor();

	const std::string& name() const noexcept { return name_; }
	/** small dense id (1..n) assigned at interning, usable as an index in validator tables */
	uint32_t id() const noexcept { return id_; }

private:
	LockClass(std::string name, uint32_t id) : name_(std::move(name)), id_(id) {}
	friend struct LockClassRegistry;

	const std::string name_;
	const uint32_t id_;
};

namespace detail::testing {
/** Runs `action` while holding the lock class registry's internal mutex (tests: cached ofType lookups must not need it). */
void withLockClassRegistryLocked(const std::function<void()>& action);
} // namespace detail::testing

} // namespace aion::gameserver::runtime

/**
 * Static lock class for a member lock: `Monitor teamLock{AION_LOCK_CLASS(GeneralTeam::teamLock)};`. The argument is stringized, so it need
 * not name an existing C++ entity. Interned once per expansion site.
 */
#define AION_LOCK_CLASS(qualifiedFieldName)                                                                                                         \
	([]() -> const ::aion::gameserver::runtime::LockClass& {                                                                                          \
		static const ::aion::gameserver::runtime::LockClass& lockClass = ::aion::gameserver::runtime::LockClass::named(#qualifiedFieldName);            \
		return lockClass;                                                                                                                               \
	}())
