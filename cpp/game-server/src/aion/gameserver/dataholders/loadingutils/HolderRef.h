#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <typeinfo>

#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::xml {

/**
 * Published static data holder (Java: `public static H X` in DataManager; docs/design/static-data.md §3.3 and the amendments §5).
 *
 * DataManager::init builds every holder, post-processes it while unpublished and then publishes it once. Afterwards the holder is immortal
 * and const: readers load the pointer lock-free from any thread (`DataManager::ITEM_DATA->getItemTemplate(id)`), and `const T*` template
 * pointers into it stay valid for the life of the process. //reload is deferred (D3); when it comes, publish will move the previous holder
 * to a retirement list that is never freed, without changing the call sites.
 *
 * Thread-safety: publish and reads may race; a reader sees either nullptr (not published yet) or the complete holder.
 */
template <class H>
class HolderRef {
public:
	constexpr HolderRef() noexcept = default;
	HolderRef(const HolderRef&) = delete;
	HolderRef& operator=(const HolderRef&) = delete;

	/** the holder. @throws runtime::NullPointerException if it is not published yet (Java: NPE on the null static field) */
	const H* operator->() const {
		const H* holder = ptr.load(std::memory_order_acquire);
		if (holder == nullptr)
			throw runtime::NullPointerException(std::string("static data holder ") + typeid(H).name() + " is not published");
		return holder;
	}
	const H& operator*() const { return *operator->(); }
	/** the holder or nullptr */
	const H* get() const noexcept { return ptr.load(std::memory_order_acquire); }
	explicit operator bool() const noexcept { return get() != nullptr; }

	/** publishes the holder once; it is never freed. @throws runtime::IllegalStateException if already published or `holder` is null */
	void publish(std::unique_ptr<H> holder) {
		if (holder == nullptr)
			throw runtime::IllegalArgumentException(std::string("cannot publish a null ") + typeid(H).name());
		const H* expected = nullptr;
		if (!ptr.compare_exchange_strong(expected, holder.get()))
			throw runtime::IllegalStateException(std::string("static data holder ") + typeid(H).name() + " is already published");
		static_cast<void>(holder.release()); // immortal
	}

	/**
	 * //reload's replacement of a published holder (Java assigns the DataManager field; owner's decision 2026-10-13, header request m5j-r1):
	 * readers see the old or the new holder. The old one is never freed - live templates, items and resolved IDREFs keep pointing into it, as
	 * Java's GC keeps the replaced objects alive (static-data.md §3.6).
	 * @throws runtime::IllegalArgumentException if `holder` is null; runtime::IllegalStateException if no holder was published before
	 */
	void republish(std::unique_ptr<H> holder) {
		if (holder == nullptr)
			throw runtime::IllegalArgumentException(std::string("cannot republish a null ") + typeid(H).name());
		const H* expected = ptr.load(std::memory_order_acquire);
		do {
			if (expected == nullptr)
				throw runtime::IllegalStateException(std::string("static data holder ") + typeid(H).name() + " was never published");
		} while (!ptr.compare_exchange_weak(expected, holder.get(), std::memory_order_acq_rel, std::memory_order_acquire));
		static_cast<void>(holder.release()); // immortal, like the holder it replaces
	}

	/** tests only: forgets the published holder (leaked, like a retired holder) so the next test can publish again */
	void resetForTests() noexcept { ptr.store(nullptr); }

private:
	std::atomic<const H*> ptr{nullptr};
};

/**
 * Published holder of the explicitly mutable families (SpawnsData, WalkerData, EventData; docs/design/static-data.md amendments §6):
 * non-const access, internally synchronized by the holder itself (runtime shims and Monitors), published once; //reload replaces EventData
 * with republish (the old holder stays alive).
 */
template <class H>
class MutableHolderRef {
public:
	constexpr MutableHolderRef() noexcept = default;
	MutableHolderRef(const MutableHolderRef&) = delete;
	MutableHolderRef& operator=(const MutableHolderRef&) = delete;

	/** @throws runtime::NullPointerException if not published yet */
	H* operator->() const {
		H* holder = ptr.load(std::memory_order_acquire);
		if (holder == nullptr)
			throw runtime::NullPointerException(std::string("static data holder ") + typeid(H).name() + " is not published");
		return holder;
	}
	H& operator*() const { return *operator->(); }
	H* get() const noexcept { return ptr.load(std::memory_order_acquire); }
	explicit operator bool() const noexcept { return get() != nullptr; }

	/** @throws runtime::IllegalStateException if already published or `holder` is null */
	void publish(std::unique_ptr<H> holder) {
		if (holder == nullptr)
			throw runtime::IllegalArgumentException(std::string("cannot publish a null ") + typeid(H).name());
		H* expected = nullptr;
		if (!ptr.compare_exchange_strong(expected, holder.get()))
			throw runtime::IllegalStateException(std::string("static data holder ") + typeid(H).name() + " is already published");
		static_cast<void>(holder.release()); // immortal
	}

	/** //reload events' replacement (header request m5j-r1), see HolderRef::republish: the old holder is kept alive */
	void republish(std::unique_ptr<H> holder) {
		if (holder == nullptr)
			throw runtime::IllegalArgumentException(std::string("cannot republish a null ") + typeid(H).name());
		H* expected = ptr.load(std::memory_order_acquire);
		do {
			if (expected == nullptr)
				throw runtime::IllegalStateException(std::string("static data holder ") + typeid(H).name() + " was never published");
		} while (!ptr.compare_exchange_weak(expected, holder.get(), std::memory_order_acq_rel, std::memory_order_acquire));
		static_cast<void>(holder.release()); // immortal, like the holder it replaces
	}

	/** tests only, see HolderRef::resetForTests */
	void resetForTests() noexcept { ptr.store(nullptr); }

private:
	std::atomic<H*> ptr{nullptr};
};

} // namespace aion::gameserver::xml
