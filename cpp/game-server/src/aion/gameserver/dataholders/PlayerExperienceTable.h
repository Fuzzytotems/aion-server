#pragma once

#include <cstdint>

#include "aion/gameserver/dataholders/PlayerExperienceTable.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.PlayerExperienceTable. @author Luno */
class PlayerExperienceTable : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/PlayerExperienceTable.xml.inc"
public:
	/**
	 * Returns the number of experience that player have at the beginning of given level.<br>
	 * For example at lv 1 it's 0
	 *
	 * @return count of experience. If <tt>level</tt> parameter is higher than the max level that player can gain, then IllegalArgumentException is
	 *         thrown.
	 */
	int64_t getStartExpForLevel(int32_t level) const;

	int32_t getLevelForExp(int64_t expValue) const;

	/**
	 * Max possible level,that player can obtain.
	 *
	 * @return max level.
	 */
	int32_t getMaxLevel() const;
};

} // namespace aion::gameserver::dataholders
