#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/dataholders/PvpExpTable.xml.h"

namespace aion::gameserver::dataholders {

/**
 * PvP XP for killing a player of the given level, and the killer's limits for gaining it, by the killer's level.
 * <p>
 * Java com.aionemu.gameserver.dataholders.PvpExpTable (upstream d725054e1). C++: rowsByLevel points into the bound rows, which stay (Java
 * drops the list after indexing it).
 *
 * @author SVDNESS
 */
class PvpExpTable : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/PvpExpTable.xml.inc"
private:
	std::vector<const Row*> rowsByLevel;

public:
	int32_t getExp(int32_t victimLevel) const;

	/**
	 * @return The accumulated PvP XP above which the player gains no more PvP XP.
	 */
	int32_t getMaxFromAllUser(int32_t level) const;

	int32_t getReduceAmount(int32_t level) const;

	int64_t getReduceIntervalMillis(int32_t level) const;

	/**
	 * @return The time after killing a player before the killer can gain PvP XP from the same victim again.
	 */
	int64_t getDelayTimeMillis(int32_t level) const;

	int32_t getMaxLevel() const;

private:
	const Row& getRow(int32_t level) const;
};

} // namespace aion::gameserver::dataholders
