#include "aion/gameserver/runtime/sync/LockClass.h"

#include <array>
#include <atomic>
#include <mutex>
#include <typeindex>
#include <unordered_map>

#include "aion/commons/utils/ClassName.h"

namespace aion::gameserver::runtime {

struct LockClassRegistry {
	// Leaked on purpose: lock classes are immortal and may be used during static destruction. A plain std::mutex (below every rank) guards
	// the tables; no callbacks or other locks are taken while holding it.
	std::mutex mutex;
	std::unordered_map<std::string, LockClass*> byName;
	std::unordered_map<std::type_index, LockClass*> byType;
	uint32_t nextId = 1;

	static LockClassRegistry& get() {
		static auto* registry = new LockClassRegistry();
		return *registry;
	}

	LockClass& intern(const std::string& name) {
		auto it = byName.find(name);
		if (it != byName.end())
			return *it->second;
		auto* lockClass = new LockClass(name, nextId++);
		byName.emplace(name, lockClass);
		return *lockClass;
	}
};

const LockClass& LockClass::named(std::string_view name) {
	LockClassRegistry& registry = LockClassRegistry::get();
	std::string key(name);
	std::scoped_lock lock(registry.mutex);
	return registry.intern(key);
}

namespace {

// Lock-free cache of dynamic lock classes (review fix: every SYNCHRONIZED on an object monitor resolved its class under the registry's global
// std::mutex, a hidden process-wide convoy under all Java synchronized sites). Open addressing keyed by the type_info address; entries are
// immutable and leaked, slots go from null to an entry exactly once, so readers need no lock. A type whose probe sequence is full is resolved
// through the registry every time (still correct).
struct TypeEntry {
	const std::type_info* type;
	const LockClass* lockClass;
};

constexpr size_t TYPE_TABLE_SIZE = 4096; // power of two
constexpr size_t TYPE_TABLE_PROBES = 32;

constinit std::array<std::atomic<const TypeEntry*>, TYPE_TABLE_SIZE> typeTable{};

size_t typeHash(const std::type_info* type) noexcept {
	auto address = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(type));
	return static_cast<size_t>((address >> 3) * 0x9E37'79B9'7F4A'7C15ull >> 40);
}

const LockClass* findCachedType(const std::type_info& type) noexcept {
	size_t hash = typeHash(&type);
	for (size_t probe = 0; probe < TYPE_TABLE_PROBES; ++probe) {
		const TypeEntry* entry = typeTable[(hash + probe) & (TYPE_TABLE_SIZE - 1)].load(std::memory_order_acquire);
		if (entry == nullptr)
			return nullptr;
		if (entry->type == &type)
			return entry->lockClass;
	}
	return nullptr;
}

void cacheType(const std::type_info& type, const LockClass& lockClass) {
	auto* entry = new TypeEntry{&type, &lockClass}; // leaked (immortal, like lock classes)
	size_t hash = typeHash(&type);
	for (size_t probe = 0; probe < TYPE_TABLE_PROBES; ++probe) {
		std::atomic<const TypeEntry*>& slot = typeTable[(hash + probe) & (TYPE_TABLE_SIZE - 1)];
		const TypeEntry* expected = nullptr;
		if (slot.compare_exchange_strong(expected, entry, std::memory_order_acq_rel))
			return;
		if (expected->type == &type)
			break; // another thread cached the same type
	}
	delete entry;
}

} // namespace

const LockClass& LockClass::ofType(const std::type_info& type) {
	if (const LockClass* cached = findCachedType(type)) [[likely]]
		return *cached;
	LockClassRegistry& registry = LockClassRegistry::get();
	LockClass* lockClass = nullptr;
	{
		std::scoped_lock lock(registry.mutex);
		if (auto it = registry.byType.find(std::type_index(type)); it != registry.byType.end())
			lockClass = it->second;
	}
	if (lockClass == nullptr) {
		std::string name = commons::utils::getClassName(type); // outside the registry mutex
		std::scoped_lock lock(registry.mutex);
		lockClass = &registry.intern(name);
		registry.byType.emplace(std::type_index(type), lockClass);
	}
	cacheType(type, *lockClass);
	return *lockClass;
}

void detail::testing::withLockClassRegistryLocked(const std::function<void()>& action) {
	std::scoped_lock lock(LockClassRegistry::get().mutex);
	action();
}

const LockClass& LockClass::anonymousMonitor() {
	static const LockClass& lockClass = named("Monitor");
	return lockClass;
}

} // namespace aion::gameserver::runtime
