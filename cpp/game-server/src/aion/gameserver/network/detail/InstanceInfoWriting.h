#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "aion/commons/utils/ByteBuffer.h"
#include "aion/commons/utils/Exception.h"
#include "aion/gameserver/model/templates/rewards/RewardItem.h"

namespace aion::gameserver::network::detail {

/**
 * C++ only, private to P4-15 (the instanceinfo score writers): the write helpers of the score writers that read no score object. The writers
 * cannot be constructed until the score classes are declared (P5-13), so the helpers live here, where the tests reach them. The buffer
 * operations are the ones of PacketWriteHelper (writeD = putInt, writeB/skip = zero bytes).
 */

/** Java ArenaScoreWriter/HarmonyScoreWriter.writeSimpleReward: [id][(int) count], or two zero ints for a null reward */
inline void writeSimpleReward(commons::utils::ByteBuffer& buf, const model::templates::rewards::RewardItem* rewardItem) {
	if (rewardItem != nullptr) {
		buf.putInt(rewardItem->getId());
		buf.putInt(static_cast<int32_t>(rewardItem->getCount())); // Java (int) narrowing keeps the low 32 bits
	} else {
		buf.putInt(0);
		buf.putInt(0);
	}
}

/**
 * Java PvpInstanceScoreWriter.writeEmptyDataToBuffer: `writeB(buf, new byte[dataSize * missingPlayerCount])`. The int product wraps like Java;
 * a negative size throws like PacketWriteHelper::skip (Java NegativeArraySizeException).
 */
inline void writeEmptyData(commons::utils::ByteBuffer& buf, int32_t dataSize, int32_t missingPlayerCount) {
	const int32_t size = static_cast<int32_t>(static_cast<uint32_t>(dataSize) * static_cast<uint32_t>(missingPlayerCount));
	if (size < 0)
		throw commons::utils::IllegalArgumentException("Negative array size: " + std::to_string(size)); // Java: NegativeArraySizeException
	const std::vector<uint8_t> zeros(static_cast<size_t>(size));
	buf.put(zeros);
}

/**
 * Java PvpInstanceScoreWriter.writePlayerBuffInfo: [0][isDead ? 60 : 0][objectId] per player, then empty 12-byte entries up to
 * maximumPlayerCount. `Players` is a range of pointer-like players (Player: isDead(), getObjectId()).
 */
template <class Players>
void writePlayerBuffInfo(commons::utils::ByteBuffer& buf, const Players& players, int32_t maximumPlayerCount) {
	int32_t count = 0;
	for (const auto& player : players) {
		buf.putInt(0); // should be instance buff ID
		buf.putInt(player->isDead() ? 60 : 0); // was previously used for remaining instance buff time
		buf.putInt(player->getObjectId());
		++count;
	}
	writeEmptyData(buf, 12, maximumPlayerCount - count);
}

} // namespace aion::gameserver::network::detail
