#pragma once

// PacketSequence (m5a-plan.md F-04, §5.8, §5.9): the matcher of the scenario gate for the order of server packets. A sequence is a list of
// elements in the plan's notation:
//   T          exactly once
//   T{n}       n times            T{a..b}   a to b times
//   T+         one or more        T*        any number
//   [T]        optional (0 or 1)  [T]{a..b} a to b times (same as T{a..b})
//   (A | B)+   alternatives: each repetition matches one of the names
// Elements are separated by commas. Packets of the async-allowed set may appear at any position: the matcher first tries to match such a
// packet against the sequence and skips it otherwise (it is still recorded and checked by the case).

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace aion::gameserver::scenario {

class PacketSequence {
public:
	struct Element {
		std::vector<std::string> names;
		int min = 1;
		/** -1: unbounded */
		int max = 1;

		bool matches(std::string_view name) const;
		std::string toString() const;
	};

	struct Result {
		bool matched = false;
		/** on failure: where the longest match stopped and what the sequence expected there */
		std::string message;
		/** on failure: the index of the first packet that could not be matched (packets.size() if the packets ended too early) */
		size_t failedAt = 0;
	};

	/** @throws std::invalid_argument for a malformed pattern */
	static PacketSequence parse(std::string_view pattern);

	PacketSequence& then(std::string name, int min = 1, int max = 1);
	PacketSequence& then(std::vector<std::string> names, int min, int max);
	PacketSequence& append(const PacketSequence& other);

	const std::vector<Element>& elements() const noexcept { return elements_; }

	/**
	 * Matches the whole packet list against the sequence.
	 *
	 * @param isAsyncAllowed
	 *          index into packets -> true if that packet may appear at any position (empty: none)
	 */
	Result match(const std::vector<std::string>& packets, const std::function<bool(size_t index)>& isAsyncAllowed = {}) const;

	std::string toString() const;

private:
	std::vector<Element> elements_;
};

} // namespace aion::gameserver::scenario
