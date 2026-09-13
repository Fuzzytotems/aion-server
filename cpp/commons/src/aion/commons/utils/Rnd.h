#pragma once

#include <array>
#include <bit>
#include <concepts>
#include <cstdint>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <type_traits>
#include <utility>

/**
 * Java: com.aionemu.commons.utils.Rnd - fast, thread-safe random numbers. Every thread uses its own generator (xoshiro256++), split off a
 * shared, randomly seeded generator via jump(), like Java's L64X256MixRandom.split(). No locking happens after a thread's first call.
 * <p>
 * Semantics follow java.util.random.RandomGenerator: <tt>bound</tt>s are exclusive, invalid bounds throw IllegalArgumentException with the
 * JDK's messages. The generated sequences differ from Java's.
 * <p>
 * Random element selection (the get overloads for ranges):
 * <pre>
 * const Npc* npc = Rnd::get(npcs);                 // lvalue range: pointer to a random element, nullptr if empty (Java: get(List), get(T[]))
 * std::optional<Point> p = Rnd::get(makePoints()); // rvalue container: the element moved out, nullopt if empty
 * int32_t npcId = Rnd::get(NPC_IDS);               // range of int32_t: the value, throws if empty (Java: get(int[]))
 * std::optional<int32_t> id = Rnd::getOptional(ids); // nullopt if empty (Java: get(List<Integer>), which returns null)
 * int32_t npcId = Rnd::get({215074, 215075});      // braced list: the value (Java: get(new int[] { ... }))
 * </pre>
 *
 * @author Balancer, Neon
 */
namespace aion::commons::utils::Rnd {

/**
 * xoshiro256++ (Blackman &amp; Vigna), a small, fast generator with excellent statistical quality and a period of 2^256 - 1. Satisfies
 * std::uniform_random_bit_generator, so it can be used with standard algorithms and distributions.
 */
class Xoshiro256PlusPlus {
public:
	using result_type = uint64_t;

	/** Seeds the four state words from the SplitMix64 sequence of the given seed (as recommended by the authors). */
	explicit Xoshiro256PlusPlus(uint64_t seed) noexcept;

	static constexpr result_type min() noexcept { return 0; }
	static constexpr result_type max() noexcept { return UINT64_MAX; }

	result_type operator()() noexcept {
		const uint64_t result = std::rotl(state[0] + state[3], 23) + state[0];
		const uint64_t t = state[1] << 17;
		state[2] ^= state[0];
		state[3] ^= state[1];
		state[1] ^= state[2];
		state[0] ^= state[3];
		state[2] ^= t;
		state[3] = std::rotl(state[3], 45);
		return result;
	}

	/** Advances the state by 2^128 calls, so the generator and its previous state produce non-overlapping sequences. */
	void jump() noexcept;

private:
	std::array<uint64_t, 4> state;
};

/**
 * @return the generator of the calling thread, e.g. for <tt>std::shuffle(v.begin(), v.end(), Rnd::generator())</tt>. Must not be shared with
 * other threads.
 */
Xoshiro256PlusPlus& generator() noexcept;

/**
 * To compare this chance with a success rate, evaluate "<tt>if (chance() &lt; success rate)</tt>" to determine a success or, alternatively
 * "<tt>if (chance() &gt;= success rate)</tt>" to determine a fail. This ensures that a success rate of 0 (0%) will always fail, and a success rate
 * of 100.0 (100%) always succeeds.
 *
 * @return A random chance between 0.0f (inclusive) and 100.0f (exclusive)
 */
float chance();

/**
 * If maxInclusive &lt; minInclusive, a warning with stack trace is logged and minInclusive is returned.
 *
 * @return A random number between minInclusive and maxInclusive
 */
int32_t get(int32_t minInclusive, int32_t maxInclusive);

/** Java: RandomGenerator.nextInt() - uniformly distributed over all int values */
int32_t nextInt();
/** Java: RandomGenerator.nextInt(bound) - [0, bound), throws IllegalArgumentException if bound &lt;= 0 */
int32_t nextInt(int32_t bound);
/** Java: RandomGenerator.nextInt(origin, bound) - [origin, bound), throws IllegalArgumentException if origin &gt;= bound */
int32_t nextInt(int32_t origin, int32_t bound);

int64_t nextLong();
/** [0, bound), throws IllegalArgumentException if bound &lt;= 0 */
int64_t nextLong(int64_t bound);
/** [origin, bound), throws IllegalArgumentException if origin &gt;= bound */
int64_t nextLong(int64_t origin, int64_t bound);

/** [0, 1) */
float nextFloat();
/** [0, bound), throws IllegalArgumentException if bound is not finite and positive */
float nextFloat(float bound);
/** [origin, bound), throws IllegalArgumentException if origin or bound are not finite or origin &gt;= bound */
float nextFloat(float origin, float bound);

/** [0, 1) */
double nextDouble();
/** [0, bound), throws IllegalArgumentException if bound is not finite and positive */
double nextDouble(double bound);
/** [origin, bound), throws IllegalArgumentException if origin or bound are not finite or origin &gt;= bound */
double nextDouble(double origin, double bound);

bool nextBoolean();

/** Java: RandomGenerator.nextBytes(byte[]) - fills the buffer with random bytes */
void nextBytes(std::span<uint8_t> bytes);

namespace detail {

/** @return a random index in [0, size), size must be positive */
size_t randomIndex(size_t size);

/** Throws IllegalArgumentException("Cannot get random int from an empty array.") or a similar message for other element types. */
[[noreturn]] void throwEmpty(bool intElements);

template <typename R>
concept SizedForwardRange = std::ranges::forward_range<R> && std::ranges::sized_range<R>;

/** Java int[]: ranges of int32_t return their values (and throw when empty), since Java has no nullable int array element */
template <typename R>
concept IntRange = SizedForwardRange<R> && std::same_as<std::ranges::range_value_t<R>, int32_t>;

/** The element can be returned by address: its reference type is an lvalue and it outlives the call (lvalue or borrowed range) */
template <typename R>
concept AddressableRange = SizedForwardRange<R> && std::is_lvalue_reference_v<std::ranges::range_reference_t<R>> &&
	(std::is_lvalue_reference_v<R> || std::ranges::borrowed_range<R>);

template <typename R>
decltype(auto) elementAt(R& range, size_t index) {
	return *std::ranges::next(std::ranges::begin(range), static_cast<std::ranges::range_difference_t<R>>(index));
}

/**
 * Infinite input range of random values in [origin, bound) (Java streams: ints(), longs(), doubles()). Like std::ranges::istream_view, each
 * element is generated exactly once, when the iterator is incremented.
 */
template <typename T>
class GeneratedView : public std::ranges::view_interface<GeneratedView<T>> {
public:
	using Generate = T (*)(T origin, T bound);

	class iterator {
	public:
		using iterator_concept = std::input_iterator_tag;
		using value_type = T;
		using difference_type = std::ptrdiff_t;

		iterator() = default;
		explicit iterator(GeneratedView* parent) noexcept : parent(parent) {}
		const T& operator*() const noexcept { return parent->current; }
		iterator& operator++() {
			parent->next();
			return *this;
		}
		void operator++(int) { ++*this; }

	private:
		GeneratedView* parent = nullptr;
	};

	GeneratedView(Generate generate, T origin, T bound) noexcept : generate(generate), origin(origin), bound(bound) {}

	iterator begin() {
		next();
		return iterator(this);
	}
	std::unreachable_sentinel_t end() const noexcept { return std::unreachable_sentinel; }

private:
	void next() { current = generate(origin, bound); }

	Generate generate;
	T origin;
	T bound;
	T current{};
};

} // namespace detail

/**
 * Java: get(List) / get(T[]).
 *
 * @return A pointer to a random element of the given range, nullptr if it's empty. The pointer is valid as long as the element is.
 */
template <detail::AddressableRange R>
	requires(!detail::IntRange<R>)
auto get(R&& range) -> decltype(std::addressof(detail::elementAt(range, 0))) {
	size_t size = static_cast<size_t>(std::ranges::size(range));
	if (size == 0)
		return nullptr;
	return std::addressof(detail::elementAt(range, detail::randomIndex(size)));
}

/**
 * Java: get(List) for temporaries, e.g. <tt>Rnd::get(filter(list))</tt>. Elements of an owning temporary cannot be returned by address, so
 * the chosen element is moved (or copied) out. Also used for ranges whose elements are computed (e.g. std::views::transform).
 *
 * @return A random element of the given range, std::nullopt if it's empty
 */
template <detail::SizedForwardRange R>
	requires(!detail::IntRange<R> && !detail::AddressableRange<R>)
std::optional<std::ranges::range_value_t<R>> get(R&& range) {
	size_t size = static_cast<size_t>(std::ranges::size(range));
	if (size == 0)
		return std::nullopt;
	return std::optional<std::ranges::range_value_t<R>>(std::move(detail::elementAt(range, detail::randomIndex(size))));
}

/**
 * Java: get(int[]).
 *
 * @return A random element from the given int32_t range (must not be empty, otherwise IllegalArgumentException is thrown)
 */
template <detail::IntRange R>
int32_t get(R&& range) {
	size_t size = static_cast<size_t>(std::ranges::size(range));
	if (size == 0)
		detail::throwEmpty(true);
	return detail::elementAt(range, size == 1 ? 0 : detail::randomIndex(size));
}

/**
 * Java: get(List&lt;Integer&gt;) and other lists whose null result is checked: a copy of a random element, or std::nullopt if the range is empty.
 * Use it for a Java List&lt;Integer&gt; ported as std::vector&lt;int32_t&gt;, since get() on a range of int32_t follows get(int[]) and throws when
 * the range is empty.
 */
template <detail::SizedForwardRange R>
std::optional<std::ranges::range_value_t<R>> getOptional(R&& range) {
	size_t size = static_cast<size_t>(std::ranges::size(range));
	if (size == 0)
		return std::nullopt;
	return std::optional<std::ranges::range_value_t<R>>(detail::elementAt(range, size == 1 ? 0 : detail::randomIndex(size)));
}

/**
 * Java: get(new int[] { ... }).
 *
 * @return A random element from the given list (must not be empty, otherwise IllegalArgumentException is thrown)
 */
template <typename T>
T get(std::initializer_list<T> values) {
	if (values.size() == 0)
		detail::throwEmpty(std::same_as<T, int32_t>);
	return values.begin()[values.size() == 1 ? 0 : detail::randomIndex(values.size())];
}

/**
 * Java: RandomGenerator.ints() - an infinite input range of random ints, generated lazily by the thread iterating it. Limit it with
 * std::views::take, e.g. <tt>for (int32_t i : Rnd::ints(0, 10) | std::views::take(5))</tt>.
 */
inline detail::GeneratedView<int32_t> ints() {
	return {[](int32_t, int32_t) { return nextInt(); }, 0, 0};
}

/** Java: RandomGenerator.ints(origin, bound). Throws IllegalArgumentException immediately if origin &gt;= bound. */
inline detail::GeneratedView<int32_t> ints(int32_t origin, int32_t bound) {
	nextInt(origin, bound); // validates the arguments now, like Java
	return {[](int32_t o, int32_t b) { return nextInt(o, b); }, origin, bound};
}

/** Java: RandomGenerator.longs() */
inline detail::GeneratedView<int64_t> longs() {
	return {[](int64_t, int64_t) { return nextLong(); }, 0, 0};
}

/** Java: RandomGenerator.longs(origin, bound). Throws IllegalArgumentException immediately if origin &gt;= bound. */
inline detail::GeneratedView<int64_t> longs(int64_t origin, int64_t bound) {
	nextLong(origin, bound);
	return {[](int64_t o, int64_t b) { return nextLong(o, b); }, origin, bound};
}

/** Java: RandomGenerator.doubles() */
inline detail::GeneratedView<double> doubles() {
	return {[](double, double) { return nextDouble(); }, 0, 0};
}

/** Java: RandomGenerator.doubles(origin, bound). Throws IllegalArgumentException immediately if the bounds are invalid. */
inline detail::GeneratedView<double> doubles(double origin, double bound) {
	nextDouble(origin, bound);
	return {[](double o, double b) { return nextDouble(o, b); }, origin, bound};
}

} // namespace aion::commons::utils::Rnd
