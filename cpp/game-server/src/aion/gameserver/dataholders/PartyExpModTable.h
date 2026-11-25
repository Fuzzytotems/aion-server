#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/dataholders/PartyExpModTable.xml.h"

namespace aion::gameserver::dataholders {

/**
 * Weights for sharing XP within a group, by how many levels a member is below the highest level member.
 * <p>
 * Java com.aionemu.gameserver.dataholders.PartyExpModTable (upstream d725054e1).
 */
class PartyExpModTable : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/PartyExpModTable.xml.inc"
private:
	int32_t columns = 0;
	std::vector<int32_t> mods;

public:
	/**
	 * The column is one past the highest level's tens digit, so the index can run into the next row.
	 *
	 * @return The weight of a member of the given level, or 0 if the member is above the highest level.
	 */
	float getWeight(int32_t memberLevel, int32_t highestLevel) const;

	int32_t size() const;
};

} // namespace aion::gameserver::dataholders
