#pragma once

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/utils/collections/ListPart.h"
#include "aion/gameserver/utils/collections/SplitList.h"
#include "aion/gameserver/utils/collections/fwd.h"

namespace aion::gameserver::utils::collections {

/**
 * SplitList implementation that will determine the partition length by a fixed max element count
 * <p>
 * C++: a class template (hub-headers.md §8.1); the inner Java class FixedElementCountListPart keeps a reference to its list (confined, fieldmap
 * K5), since a part never outlives the partitioning.
 *
 * @author Sykra, Neon
 */
template <class Type>
class FixedElementCountSplitList : public SplitList<Type> {
private:
	const int32_t maxElementCount;

public:
	/**
	 * @param listToSplit
	 *          List of elements to split
	 * @param oneTimeSplitOnEmptyData
	 *          true if an empty list should produce one split with an empty list
	 * @param maxElementCount
	 *          fixed maximum element count used for a partition. It needs to be greater than 0 otherwise an {@link IllegalArgumentException} is
	 *          thrown
	 */
	FixedElementCountSplitList(std::vector<SplitElement<Type>> listToSplit, bool oneTimeSplitOnEmptyData, int32_t maxElementCountValue)
		: SplitList<Type>(std::move(listToSplit), oneTimeSplitOnEmptyData), maxElementCount(maxElementCountValue) {
		if (maxElementCountValue <= 0)
			throw runtime::IllegalArgumentException("maxElementCount needs to be larger than 0");
	}

protected:
	std::unique_ptr<ListPart<Type>> newListPart(int32_t partNo, bool isLast) override {
		return std::make_unique<FixedElementCountListPart>(*this, partNo, isLast);
	}

private:
	class FixedElementCountListPart : public ListPart<Type> {
	public:
		FixedElementCountListPart(const FixedElementCountSplitList& outerList, int32_t partNo, bool isLast)
			: ListPart<Type>(partNo, isLast), outer(outerList) {}

	protected:
		bool fits(const SplitElement<Type>&) override { return this->size() < outer.maxElementCount; }

	private:
		const FixedElementCountSplitList& outer; // confined: the part lives only while the list is partitioned (Java: this$0)
	};
};

} // namespace aion::gameserver::utils::collections
