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
 * One pinned object: the RefCounted to retain (the object itself, or a part's owner), or nothing for immortals and templates.
 * Constructed implicitly from pointers and Refs so that `{this, &effect}` works.
 */
class PinTarget {
public:
	template <Pinnable T>
	PinTarget(T* target) : PinTarget(classify(target)) {}
	template <Pinnable T>
	PinTarget(const Ref<T>& target) : PinTarget(target.get()) {}

	/** the object to retain, or nullptr for immortals/templates/null */
	const RefCounted* retained() const noexcept { return retained_; }

private:
	struct Classified {
		const RefCounted* retained;
	};
	explicit PinTarget(Classified classified) noexcept : retained_(classified.retained) {}

	static Classified classify(const RefCounted* object) noexcept { return {object}; }
	static Classified classify(const OwnedPart* part) noexcept { return {part != nullptr ? &part->partOwner() : nullptr}; }
	/** checked builds (C10): throws IllegalStateException if the address is not a registered Immortal */
	static Classified classify(const Immortal* immortal);
	static Classified classify(const StaticTemplate*) noexcept { return {nullptr}; }
	template <class T>
		requires(!std::derived_from<std::remove_cv_t<T>, RefCounted> && !std::derived_from<std::remove_cv_t<T>, OwnedPart> &&
			!std::derived_from<std::remove_cv_t<T>, Immortal> && !std::derived_from<std::remove_cv_t<T>, StaticTemplate> && IsStaticTemplate<std::remove_cv_t<T>>::value)
	static Classified classify(T*) noexcept {
		return {nullptr};
	}

	const RefCounted* retained_ = nullptr;
};

/**
 * The owners a task or stored callback keeps alive while it is pending (design §7.1, §7.3): up to MAX_OWNERS RefCounted objects (retained
 * for the Pin's lifetime), plus any number of immortals and templates (checked only).
 *
 * `schedule(this, [this] {...}, delay)`, `scheduleAtFixedRate({this, &effect}, [this, &effect] {...}, delay, period)`: every `this`/`&name`
 * captured by a pinned lambda must be in its pin list (lint L5). A part (AI, controller) pins its owner. The Future releases its Pin when the
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
	/** @throws IllegalArgumentException if more than MAX_OWNERS targets retain an object */
	Pin(std::initializer_list<PinTarget> targets);
	Pin(const Pin& other) noexcept;
	Pin(Pin&& other) noexcept;
	Pin& operator=(const Pin& other) noexcept;
	Pin& operator=(Pin&& other) noexcept;
	~Pin();

	/** Releases every retained owner (idempotent). */
	void reset() noexcept;

	/** true if `owner` is one of the retained owners (tasksPinning, leak census). */
	bool pins(const RefCounted& owner) const noexcept;
	/** number of retained owners */
	size_t size() const noexcept { return count_; }
	const RefCounted* owner(size_t index) const noexcept { return index < count_ ? owners_[index] : nullptr; }

private:
	std::array<const RefCounted*, MAX_OWNERS> owners_{};
	size_t count_ = 0;
};

} // namespace aion::gameserver::runtime
