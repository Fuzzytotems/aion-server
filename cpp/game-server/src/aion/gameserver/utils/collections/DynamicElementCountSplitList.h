#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/sched/PinnedCallback.h"
#include "aion/gameserver/utils/collections/ListPart.h"
#include "aion/gameserver/utils/collections/SplitList.h"
#include "aion/gameserver/utils/collections/fwd.h"

namespace aion::gameserver::utils::collections {

/**
 * SplitList implementation that will dynamically determine the partition size based on the actual length of the elements contained in the list.
 * <p>
 * C++: a class template (hub-headers.md §8.1). The length calculator is Java's `Function<Type, Integer>`, a PinnedCallback taking the element by
 * reference like the packets' DYNAMIC_BODY_PART_SIZE_CALCULATOR constants. The inner Java class DynamicElementCountListPart keeps a reference
 * to its list (confined, fieldmap K5).
 *
 * @author Sykra, Neon
 */
template <class Type>
class DynamicElementCountSplitList : public SplitList<Type> {
private:
	const runtime::PinnedCallback<int32_t(Type&)> lengthCalculator;
	const int32_t maxLength;

public:
	/**
	 * @param listToSplit
	 *          List of elements to split
	 * @param oneTimeSplitOnEmptyData
	 *          true if an empty list should produce one split with an empty list
	 * @param maxLength
	 *          maximum length of one split
	 * @param lengthCalculator
	 *          {@link Function} that will calculate the size length of an element
	 * @throws IllegalArgumentException
	 *           if maxLength is not larger than 0
	 */
	DynamicElementCountSplitList(std::vector<SplitElement<Type>> listToSplit, bool oneTimeSplitOnEmptyData, int32_t maxLengthValue,
		runtime::PinnedCallback<int32_t(Type&)> lengthCalculatorValue)
		: SplitList<Type>(std::move(listToSplit), oneTimeSplitOnEmptyData), lengthCalculator(std::move(lengthCalculatorValue)),
		  maxLength(maxLengthValue) {
		if (this->maxLength <= 0)
			throw runtime::IllegalArgumentException("maxLength needs to be larger than 0");
	}

protected:
	std::unique_ptr<ListPart<Type>> newListPart(int32_t partNo, bool isLast) override {
		return std::make_unique<DynamicElementCountListPart>(*this, partNo, isLast);
	}

private:
	/** The element as the length calculator's `Type&` argument */
	static Type& elementReference(const SplitElement<Type>& element) {
		if constexpr (runtime::Retainable<Type>)
			return *element;
		else
			return const_cast<Type&>(element); // a confined value element; Java passes the object itself
	}

	class DynamicElementCountListPart : public ListPart<Type> {
	private:
		const DynamicElementCountSplitList& outer; // confined: the part lives only while the list is partitioned (Java: this$0)
		int32_t currentLength = 0;

	public:
		DynamicElementCountListPart(const DynamicElementCountSplitList& outerList, int32_t partNo, bool isLast)
			: ListPart<Type>(partNo, isLast), outer(outerList) {}

		bool add(const SplitElement<Type>& type) override {
			if (ListPart<Type>::add(type)) {
				currentLength += outer.lengthCalculator(elementReference(type));
				return true;
			}
			return false;
		}

	protected:
		bool fits(const SplitElement<Type>& element) override {
			int32_t elementLength = outer.lengthCalculator(elementReference(element));
			if (elementLength < 0)
				throw runtime::IllegalStateException("elementLength(" + std::to_string(elementLength) + ") cannot be lesser than 0");
			if (elementLength > outer.maxLength) {
				throw runtime::IllegalStateException("elementLength(" + std::to_string(elementLength) + ") is greater than the maxLength (" +
					std::to_string(outer.maxLength) + ")");
			}
			return elementLength + currentLength <= outer.maxLength;
		}
	};
};

} // namespace aion::gameserver::utils::collections
