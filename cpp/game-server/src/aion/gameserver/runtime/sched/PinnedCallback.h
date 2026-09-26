#pragma once

#include <concepts>
#include <memory>
#include <source_location>
#include <type_traits>
#include <typeinfo>
#include <utility>

#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/sched/Pin.h"
#include "aion/gameserver/runtime/sched/TaskConcepts.h"

namespace aion::gameserver::runtime {

template <class Signature>
class PinnedCallback;

/**
 * Type-erased stored callback with the same capture rules as tasks (design §7.1, §7.3, RR-3): ActionObserver lambdas, DeathObserver,
 * RequestResponseHandler, CronService jobs, Event end tasks.
 *
 * - `PinnedCallback<void(Creature&)>(Pin{this, &player}, [this, &player](Creature& c) {...})`: the Pin retains its owners for as long as the
 *   callback (or any copy) exists; lint L5 checks that every `this`/`&name` capture is pinned.
 * - `PinnedCallback<void()>(captureless / TaskStruct / bindTask(...))`: unpinned forms, checked at compile time (UnpinnedCallback).
 * - Copies share one immutable implementation (cheap copy, identity preserved: operator== compares the implementation, so a callback can be
 *   found and removed again, like Java object identity of a Runnable).
 * - Invocation is const and may happen concurrently on several threads; the stored callable must tolerate that (Java lambdas do).
 * - Invoking an empty callback throws NullPointerException. reset() drops the reference (the Pin is released when the last copy is gone).
 * - target_type() is the stored callable's type (CronService.findJobs by class); target<T>() gives typed read access to the stored callable
 *   when its type is exactly T (like std::function::target; no subtype match), e.g. SiegeService reading the location id of a
 *   SiegeStartRunnable job found by CronService.findNextFireTimes.
 * Thread-safety: a PinnedCallback object is a value (do not write one object concurrently); invocation of copies is thread-safe.
 */
template <class R, class... Args>
class PinnedCallback<R(Args...)> {
public:
	PinnedCallback() noexcept = default;
	PinnedCallback(std::nullptr_t) noexcept {}

	template <class F>
		requires std::is_invocable_r_v<R, const std::decay_t<F>&, Args...>
	PinnedCallback(Pin pin, F&& callable, std::source_location where = std::source_location::current())
		: impl(std::make_shared<Model<std::decay_t<F>>>(std::move(pin), std::forward<F>(callable), TaskInfo{where, TaskKind::CALLBACK_})) {}

	template <class F>
		requires UnpinnedCallback<F, R, Args...> && (!std::same_as<std::remove_cvref_t<F>, PinnedCallback>)
	PinnedCallback(F&& callable, std::source_location where = std::source_location::current())
		: impl(std::make_shared<Model<std::decay_t<F>>>(Pin(), std::forward<F>(callable), TaskInfo{where, TaskKind::CALLBACK_})) {}

	/** @throws NullPointerException if empty; exceptions of the callable propagate */
	R operator()(Args... args) const {
		if (!impl)
			throw NullPointerException("Invoking an empty PinnedCallback");
		return impl->invoke(std::forward<Args>(args)...);
	}

	explicit operator bool() const noexcept { return impl != nullptr; }
	void reset() noexcept { impl.reset(); }

	const TaskInfo& getTaskInfo() const noexcept {
		static constexpr TaskInfo none{};
		return impl ? impl->info : none;
	}
	const std::type_info& target_type() const noexcept { return impl ? impl->type() : typeid(void); }
	/** The stored callable if its (decayed) type is exactly T, else nullptr. Shared by all copies; invocation may run concurrently. */
	template <class T>
	const T* target() const noexcept {
		if constexpr (!std::is_invocable_r_v<R, const T&, Args...>) {
			return nullptr; // cannot be the stored callable (and Model<T> must not be instantiated)
		} else {
			if (!impl || impl->type() != typeid(T))
				return nullptr;
			return &static_cast<const Model<T>*>(impl.get())->callable;
		}
	}
	bool pins(const RefCounted& owner) const noexcept { return impl && impl->pin.pins(owner); }

	friend bool operator==(const PinnedCallback& a, const PinnedCallback& b) noexcept { return a.impl == b.impl; }
	friend bool operator==(const PinnedCallback& a, std::nullptr_t) noexcept { return a.impl == nullptr; }

private:
	struct Concept {
		Concept(Pin pin, const TaskInfo& info) : pin(std::move(pin)), info(info) {}
		virtual ~Concept() = default;
		virtual R invoke(Args&&... args) const = 0;
		virtual const std::type_info& type() const noexcept = 0;
		Pin pin;
		const TaskInfo info;
	};

	template <class F>
	struct Model final : Concept {
		Model(Pin pin, F callable, const TaskInfo& info) : Concept(std::move(pin), info), callable(std::move(callable)) {}
		R invoke(Args&&... args) const override { return static_cast<R>(std::invoke(callable, std::forward<Args>(args)...)); }
		const std::type_info& type() const noexcept override { return typeid(F); }
		F callable;
	};

	std::shared_ptr<const Concept> impl;
};

} // namespace aion::gameserver::runtime
