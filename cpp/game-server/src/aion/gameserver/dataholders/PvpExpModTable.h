#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/dataholders/PvpExpModTable.xml.h"

namespace aion::gameserver::dataholders {

/**
 * PvP XP multipliers by the level difference between killer and victim. Each row holds one column per ten killer levels.
 * <p>
 * Java com.aionemu.gameserver.dataholders.PvpExpModTable (upstream d725054e1).
 *
 * @author SVDNESS
 */
class PvpExpModTable : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/PvpExpModTable.xml.inc"
private:
	int32_t minLevelDiff = 0;
	std::vector<std::vector<int32_t>> modsByLevelDiff;

public:
	/**
	 * Level differences outside the table use its first or last row.
	 */
	float getMultiplier(int32_t killerLevel, int32_t victimLevel) const;

	int32_t size() const;
};

} // namespace aion::gameserver::dataholders
