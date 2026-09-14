#pragma once

#include <concepts>
#include <cstddef>
#include <memory>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"

namespace aion::gameserver::runtime {

class Future;

/**
 * Compile-time capture rules for unpinned tasks and stored callbacks (design §7.1, §7.3 "Compile time").
 *
 * A value may be stored in an unpinned task (a TaskStruct member or a bindTask argument) if it is a TaskArg:
 * arithmetic, enum, Ref<X> (incl. FutureRef), std::string, a pointer to a const static template (IsTemplatePtr), a pointer to an Immortal
 * (IsImmortalPtr), std::shared_ptr<const X> (IsSharedPtrToConst), or a type enabled by TaskArgExtension (network/ enables
 * std::weak_ptr<AionConnection>; the kernel cannot name AionConnection).
 * Ptr<X>, raw pointers to RefCounted objects, references and std::string_view are never TaskArgs.
 */
template <class T>
struct TaskArgExtension : std::false_type {};

template <class T>
concept IsTemplatePtr =
	std::is_pointer_v<T> && std::is_const_v<std::remove_pointer_t<T>> && IsStaticTemplate<std::remove_cv_t<std::remove_pointer_t<T>>>::value;

template <class T>
concept IsImmortalPtr = std::is_pointer_v<T> && std::derived_from<std::remove_cv_t<std::remove_pointer_t<T>>, Immortal>;

template <class T>
struct IsSharedPtrToConstType : std::false_type {};
template <class X>
struct IsSharedPtrToConstType<std::shared_ptr<const X>> : std::true_type {};
template <class T>
concept IsSharedPtrToConst = IsSharedPtrToConstType<T>::value;

template <class T>
concept TaskArg = std::is_arithmetic_v<T> || std::is_enum_v<T> || IsRef<T> || std::same_as<T, std::string> || IsTemplatePtr<T> ||
	IsImmortalPtr<T> || IsSharedPtrToConst<T> || TaskArgExtension<T>::value;

/** Base of aggregate task structs whose members are all TaskArgs: `struct GeneralUpdateTask : TaskStruct { int32_t playerId; void operator()() const; };` */
struct TaskStruct {};

namespace detail {

/** converts to anything (used to count aggregate initializers); never evaluated */
struct AnyInitializer {
	template <class U>
	operator U() const;
};

/** converts only to TaskArg types and to the TaskStruct base; never evaluated */
struct TaskArgInitializer {
	template <class U>
		requires(TaskArg<std::remove_cvref_t<U>> || std::same_as<std::remove_cvref_t<U>, TaskStruct>) && (!std::is_reference_v<U>)
	operator U() const;
};

template <class T, class... Args>
concept BraceInitializableFrom = requires { T{std::declval<Args>()...}; };

template <class T, class... Initializers>
constexpr size_t aggregateInitializerCount() {
	if constexpr (sizeof...(Initializers) > 32)
		return sizeof...(Initializers);
	else if constexpr (BraceInitializableFrom<T, Initializers..., AnyInitializer>)
		return aggregateInitializerCount<T, Initializers..., AnyInitializer>();
	else
		return sizeof...(Initializers);
}

template <class T, size_t N, class... Initializers>
constexpr bool initializableFromTaskArgs() {
	if constexpr (sizeof...(Initializers) == N)
		return BraceInitializableFrom<T, Initializers...>;
	else
		return initializableFromTaskArgs<T, N, Initializers..., TaskArgInitializer>();
}

} // namespace detail

/**
 * The design's `AllFieldsAre<F, TaskArg>` (C++23 has no concept template parameters): F is an aggregate deriving TaskStruct whose members are
 * all TaskArgs. Checked by counting aggregate initializers and requiring F to be brace-initializable from initializers that convert only to
 * TaskArg types (a Ptr, reference, raw object pointer or string_view member fails).
 */
template <class F>
concept AllFieldsAreTaskArgs = std::is_aggregate_v<F> && std::derived_from<F, TaskStruct> &&
	detail::initializableFromTaskArgs<F, detail::aggregateInitializerCount<F>()>();

/** Result of bindTask: a captureless function plus TaskArg arguments. */
template <class F, class... A>
class BoundTask {
public:
	BoundTask(F function, std::tuple<A...> arguments) : function(std::move(function)), arguments(std::move(arguments)) {}

	/**
	 * Invokes the function with each stored argument. A Ref<X> argument is passed as an adapter convertible to X& (NullPointerException if null),
	 * Ptr<X> and const Ref<X>&, so `[](Npc& claw) {...}` and `[](Ptr<Npc> claw) {...}` both work; other arguments are passed as const lvalues.
	 * Extra leading parameters (e.g. Future& for periodic tasks) are forwarded first.
	 */
	template <class... Leading>
	decltype(auto) operator()(Leading&&... leading) const;

private:
	F function;
	std::tuple<A...> arguments;
};

template <class T>
struct IsBoundTaskType : std::false_type {};
template <class F, class... A>
struct IsBoundTaskType<BoundTask<F, A...>> : std::true_type {};
template <class T>
concept IsBoundTask = IsBoundTaskType<std::remove_cvref_t<T>>::value;

/** A lambda (or function object) without captures. */
template <class F>
concept Captureless = std::is_empty_v<std::remove_cvref_t<F>> && std::is_trivially_copyable_v<std::remove_cvref_t<F>>;

/**
 * Binds TaskArg arguments to a captureless function: `schedule(bindTask([](Npc& claw) { claw.getController().delete_(); }, Ref<Npc>(claw)), 300000)`.
 * The compile error names the offending argument type when it is not a TaskArg.
 */
template <class F, class... A>
	requires Captureless<F> && (TaskArg<std::decay_t<A>> && ...)
auto bindTask(F captureless, A&&... args) {
	return BoundTask<std::decay_t<F>, std::decay_t<A>...>(std::move(captureless), std::tuple<std::decay_t<A>...>(std::forward<A>(args)...));
}

/**
 * Unpinned task (design §7.1): a captureless lambda or function pointer, a TaskStruct with TaskArg members, or a bindTask result.
 * UnpinnedTask: callable without arguments; UnpinnedPeriodicTask: callable with Future& (fixed-rate bodies that cancel themselves).
 */
template <class F>
concept UnpinnedTask = std::convertible_to<std::decay_t<F>, void (*)()> ||
	(std::derived_from<std::remove_cvref_t<F>, TaskStruct> && AllFieldsAreTaskArgs<std::remove_cvref_t<F>> && std::invocable<const std::remove_cvref_t<F>&>) ||
	(IsBoundTask<F> && std::invocable<const std::remove_cvref_t<F>&>);

template <class F>
concept UnpinnedPeriodicTask = std::convertible_to<std::decay_t<F>, void (*)(Future&)> ||
	(std::derived_from<std::remove_cvref_t<F>, TaskStruct> && AllFieldsAreTaskArgs<std::remove_cvref_t<F>> &&
		std::invocable<const std::remove_cvref_t<F>&, Future&>) ||
	(IsBoundTask<F> && std::invocable<const std::remove_cvref_t<F>&, Future&>);

/** Unpinned stored callback with signature R(Args...) (PinnedCallback without a Pin, design §7.1 RR-3). */
template <class F, class R, class... Args>
concept UnpinnedCallback = std::convertible_to<std::decay_t<F>, R (*)(Args...)> ||
	(std::derived_from<std::remove_cvref_t<F>, TaskStruct> && AllFieldsAreTaskArgs<std::remove_cvref_t<F>> &&
		std::is_invocable_r_v<R, const std::remove_cvref_t<F>&, Args...>) ||
	(IsBoundTask<F> && std::is_invocable_r_v<R, const std::remove_cvref_t<F>&, Args...>);

// ------------------------------------------------------------------------------------------------------------------------------------------

namespace detail {

template <class X>
class RefArgument {
public:
	explicit RefArgument(const Ref<X>& ref) noexcept : ref(ref) {}
	operator X&() const { return *ref; }
	operator Ptr<X>() const noexcept { return Ptr<X>(ref); }
	operator const Ref<X>&() const noexcept { return ref; }

private:
	const Ref<X>& ref;
};

template <class A>
const A& adaptArgument(const A& argument) noexcept {
	return argument;
}

template <class X>
RefArgument<X> adaptArgument(const Ref<X>& argument) noexcept {
	return RefArgument<X>(argument);
}

} // namespace detail

template <class F, class... A>
template <class... Leading>
decltype(auto) BoundTask<F, A...>::operator()(Leading&&... leading) const {
	return std::apply([&](const A&... stored) -> decltype(auto) { return function(std::forward<Leading>(leading)..., detail::adaptArgument(stored)...); },
		arguments);
}

} // namespace aion::gameserver::runtime
