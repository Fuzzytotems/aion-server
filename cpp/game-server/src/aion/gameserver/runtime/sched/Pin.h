#pragma once

#include <array>
#include <concepts>
#include <cstddef>
#include <initializer_list>
#include <type_traits>

#include "aion/gameserver/runtime/lifetime/Parts.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"

namespace aion::gameserver::runtime {

/** Objects a task or stored callback may pin (design §7.1): RefCounted, OwnedPart (pins its owner), Immortal, static data templates. */
template <class T>
concept Pinnable = std::derived_from<std::remove_cv_t<T>, RefCounted> || std::derived_from<std::remove_cv_t<T>, OwnedPart> ||
	std::derived_from<std::remove_cv_t<T>, Immortal> || IsStaticTemplate<std::remove_cv_t<T>>::value;

/**
 * One pinned object: the RefCounted to retain (the object itself, or a part's owner) plus, for an OwnedPart, the part itself; nothing for
 * immortals and templates. Constructed implicitly from pointers and Refs so that `{this, &effect}` works.
 */
class PinTarget {
public:
	template <Pinnable T>
	PinTarget(T* target) : PinTarget(classify(target)) {}
	template <Pinnable T>
	PinTarget(const Ref<T>& target) : PinTarget(target.get()) {}

	/** the object to retain (a part's owner for a part), or nullptr for immortals/templates/null */
	const RefCounted* retained() const noexcept { return retained_; }
	/** the pinned part, or nullptr if the target is not an OwnedPart */
	const OwnedPart* part() const noexcept { return part_; }

private:
	struct Classified {
		const RefCounted* retained;
		const OwnedPart* part = nullptr;
	};
	explicit PinTarget(Classified classified) noexcept : retained_(classified.retained), part_(classified.part) {}

	static Classified classify(const RefCounted* object) noexcept { return {object}; }
	static Classified classify(const OwnedPart* part) noexcept { return {part != nullptr ? &part->partOwner() : nullptr, part}; }
	/** checked builds (C10): throws IllegalStateException if the address is not a registered Immortal */
	static Classified classify(const Immortal* immortal);
	/**
	 * Static data templates (StaticTemplate base or an IsStaticTemplate specialization). A RefCounted class deriving StaticTemplate
	 * (PlayerCommonData) is no template (IsStaticTemplate is false) and takes the RefCounted overload: `Pin(pcd)` retains it.
	 */
	template <class T>
		requires(!std::derived_from<std::remove_cv_t<T>, RefCounted> && !std::derived_from<std::remove_cv_t<T>, OwnedPart> &&
			!std::derived_from<std::remove_cv_t<T>, Immortal> && IsStaticTemplate<std::remove_cv_t<T>>::value)
	static Classified classify(T*) noexcept {
		return {nullptr};
	}

	const RefCounted* retained_ = nullptr;
	const OwnedPart* part_ = nullptr;
};

/**
 * The owners a task or stored callback keeps alive while it is pending (design §7.1, §7.3): up to MAX_OWNERS RefCounted objects (retained
 * for the Pin's lifetime), plus any number of immortals and templates (checked only).
 *
 * `schedule(this, [this] {...}, delay)`, `scheduleAtFixedRate({this, &effect}, [this, &effect] {...}, delay, period)`: every `this`/`&name`
 * captured by a pinned lambda must be in its pin list (lint L5). A part (AI, controller, storage) pins its owner and the part itself
 * (OwnedPart::retain), so a part replaced in a PartSlot<RECLAIMER> or PartMap while the task is pending is not destroyed before the task ends.
 * An owner and its part share one slot (the part's retain keeps the owner alive); two parts of one owner take two slots. The Future releases its Pin when the
 * task finishes or is cancelled (deviation 5: cancelled tasks release their captures immediately).
 *
 * Copying retains again; moving transfers. Thread-safety: like a value type (do not share one Pin object between threads while writing it).
 */
class Pin {
public:
	static constexpr size_t MAX_OWNERS = 4;

	Pin() noexcept = default;
	template <Pinnable T>
	Pin(T* target) : Pin({PinTarget(target)}) {}
	template <Pinnable T>
	Pin(const Ref<T>& target) : Pin({PinTarget(target)}) {}
	/** @throws IllegalArgumentException if the targets need more than MAX_OWNERS slots */
	Pin(std::initializer_list<PinTarget> targets);
	Pin(const Pin& other) noexcept;
	Pin(Pin&& other) noexcept;
	Pin& operator=(const Pin& other) noexcept;
	Pin& operator=(Pin&& other) noexcept;
	~Pin();

	/** Releases every retained owner (idempotent). */
	void reset() noexcept;

	/** true if `owner` is one of the retained owners, directly or as the owner of a pinned part (tasksPinning, leak census). */
	bool pins(const RefCounted& owner) const noexcept;
	/** number of slots (retained owners; two parts of one owner count twice) */
	size_t size() const noexcept { return count_; }
	/** the owner of slot `index` (a pinned part's owner), nullptr past size() */
	const RefCounted* owner(size_t index) const noexcept { return index < count_ ? owners_[index] : nullptr; }
	/** the part of slot `index`, nullptr if the slot pins the owner only */
	const OwnedPart* part(size_t index) const noexcept { return index < count_ ? parts_[index] : nullptr; }

private:
	void retainSlot(size_t index) const noexcept;

	std::array<const RefCounted*, MAX_OWNERS> owners_{};
	/** per slot: the retained part (whose retain also holds the owner), or nullptr when the owner itself is retained */
	std::array<const OwnedPart*, MAX_OWNERS> parts_{};
	size_t count_ = 0;
};

} // namespace aion::gameserver::runtime
