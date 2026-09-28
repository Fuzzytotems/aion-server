#pragma once

#include <cstdint>
#include <utility>
#include <vector>

#include "aion/gameserver/runtime/sched/PinnedCallback.h"
#include "aion/gameserver/utils/collections/DynamicElementCountSplitList.h"
#include "aion/gameserver/utils/collections/fwd.h"

namespace aion::gameserver::utils::collections {

namespace detail {
/** Java: AionServerPacket.MAX_USABLE_PACKET_BODY_SIZE (defined in the .cpp, so this header needs no packet header) */
int32_t maxUsablePacketBodySize() noexcept;
} // namespace detail

/**
 * This class is a more specialized version of the {@link DynamicElementCountSplitList}. It will utilize the maximum server packet body byte size to
 * determine how many elements can be included in a partition. The maximum usable byte size is dynamically determined by the MAX_BODY_SIZE and the
 * given static body byte length.
 *
 * @author Sykra, Neon
 */
template <class Type>
class DynamicServerPacketBodySplitList : public DynamicElementCountSplitList<Type> {
public:
	/**
	 * @param listToSplit
	 *          List of elements to split
	 * @param oneTimeSplitOnEmptyData
	 *          true if an empty list should produce one split with an empty list
	 * @param staticBodyByteSize
	 *          static server packet body size, that will be subtracted from the maximum body size to determine the usable body size in bytes
	 * @param byteLengthCalculator
	 *          {@link Function} that will calculate the byte length of one element
	 */
	DynamicServerPacketBodySplitList(std::vector<SplitElement<Type>> listToSplit, bool oneTimeSplitOnEmptyData, int32_t staticBodyByteSize,
		runtime::PinnedCallback<int32_t(Type&)> byteLengthCalculator)
		: DynamicElementCountSplitList<Type>(std::move(listToSplit), oneTimeSplitOnEmptyData,
			  detail::maxUsablePacketBodySize() - staticBodyByteSize, std::move(byteLengthCalculator)) {}
};

} // namespace aion::gameserver::utils::collections
