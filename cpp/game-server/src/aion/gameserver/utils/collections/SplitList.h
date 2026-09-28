#pragma once

#include <cstddef>
#include <iterator>
#include <memory>
#include <utility>
#include <vector>

#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/utils/collections/ListPart.h"
#include "aion/gameserver/utils/collections/fwd.h"

namespace aion::gameserver::utils::collections {

/**
 * Generic list splitter that will split a given list into multiple partitions. The length of a single partition needs to be determined by an
 * implementation.
 * <p>
 * C++: a class template (hub-headers.md §8.1); the list is taken by value (Java keeps the caller's list). Java's `Iterable<ListPart<Type>>`
 * is `iterator()` (hasNext/next, like BrokerService's `iterator().next()`) plus `begin()`/`end()` for range-for; each iteration partitions the
 * list anew, as Java's iterator() does. Confined (fieldmap K5).
 *
 * @author xTz, Rolandas, Sykra, Neon
 */
template <class Type>
class SplitList {
public:
	using Element = SplitElement<Type>;
	using Parts = std::vector<std::unique_ptr<ListPart<Type>>>;

	/** Java: Iterator<ListPart<Type>> over one partitioning of the list, usable with range-for */
	class PartIterator {
	public:
		using value_type = ListPart<Type>;
		using difference_type = std::ptrdiff_t;

		PartIterator() = default;
		explicit PartIterator(std::shared_ptr<const Parts> parts) : parts(std::move(parts)) {}

		bool hasNext() const { return parts && index < parts->size(); }

		/** @throws NoSuchElementException if there are no more parts */
		ListPart<Type>& next() {
			if (!hasNext())
				throw runtime::NoSuchElementException("SplitList has no more parts");
			return *(*parts)[index++];
		}

		ListPart<Type>& operator*() const { return *(*parts)[index]; }
		PartIterator& operator++() {
			++index;
			return *this;
		}
		void operator++(int) { ++index; }
		friend bool operator==(const PartIterator& it, std::default_sentinel_t) noexcept { return !it.hasNext(); }

	private:
		std::shared_ptr<const Parts> parts;
		size_t index = 0;
	};

private:
	const std::vector<Element> listToSplit;
	const bool oneTimeSplitOnEmptyData;

public:
	/**
	 * @param listToSplit
	 *          List of elements to split
	 * @param oneTimeSplitOnEmptyData
	 *          true if an empty list should produce one split with an empty list
	 */
	SplitList(std::vector<Element> listToSplitValue, bool oneTimeSplitOnEmptyDataValue)
		: listToSplit(std::move(listToSplitValue)), oneTimeSplitOnEmptyData(oneTimeSplitOnEmptyDataValue) {}

	virtual ~SplitList() = default;
	SplitList(const SplitList&) = delete;
	SplitList& operator=(const SplitList&) = delete;

private:
	Parts partitionList() {
		Parts parts;
		if (listToSplit.empty() && oneTimeSplitOnEmptyData) {
			parts.push_back(newListPart(1, true));
			return parts;
		} else if (listToSplit.empty()) {
			return parts;
		}
		size_t startIndex = 0;
		int32_t partNo = 1;
		while (startIndex < listToSplit.size()) {
			std::unique_ptr<ListPart<Type>> listPart = newListPart(partNo++, false);
			while (startIndex < listToSplit.size() && listPart->fits(listToSplit[startIndex]))
				listPart->add(listToSplit[startIndex++]);
			parts.push_back(std::move(listPart));
		}
		if (!parts.empty()) {
			ListPart<Type>& types = *parts.back();
			types.setLast(true);
		}
		return parts;
	}

public:
	/** Java: iterator() */
	PartIterator iterator() { return PartIterator(std::make_shared<const Parts>(partitionList())); }

	/** C++ only: range-for over a fresh partitioning (hub-headers.md §7.2) */
	PartIterator begin() { return iterator(); }
	std::default_sentinel_t end() const noexcept { return {}; }

protected:
	virtual std::unique_ptr<ListPart<Type>> newListPart(int32_t partNo, bool isLast) = 0;
};

} // namespace aion::gameserver::utils::collections
