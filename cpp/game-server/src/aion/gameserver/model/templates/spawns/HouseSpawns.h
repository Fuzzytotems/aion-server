#pragma once

#include <vector>

#include "aion/gameserver/model/templates/spawns/HouseSpawns.xml.h"

namespace aion::gameserver::model::templates::spawns {

/** Java com.aionemu.gameserver.model.templates.spawns.HouseSpawns. @author Rolandas */
class HouseSpawns : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/spawns/HouseSpawns.xml.inc"
public:
	/** @return the spawns (empty for Java's Collections.emptyList() when there are none) */
	const std::vector<HouseSpawn>& getSpawns() const { return spawns; }
};

} // namespace aion::gameserver::model::templates::spawns
