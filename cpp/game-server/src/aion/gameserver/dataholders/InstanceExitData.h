#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "aion/gameserver/dataholders/InstanceExitData.xml.h"
#include "aion/gameserver/model/fwd.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.InstanceExitData.
 * <p>
 * C++: the lists point into the bound `instanceExit` storage, which stays after afterUnmarshal (static-data.md §2.6).
 *
 * @author xTz
 */
class InstanceExitData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/InstanceExitData.xml.inc"
private:
	std::unordered_map<int32_t, std::vector<const model::templates::portal::InstanceExit*>> instanceExitByWorldId;

public:
	/** @return the first exit of the world for the race (or for all races), nullptr (Java null) if there is none */
	const model::templates::portal::InstanceExit* getInstanceExit(int32_t worldId, model::Race race) const;

	int32_t size() const;
};

} // namespace aion::gameserver::dataholders
