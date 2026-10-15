#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/dataholders/GlobalDropData.xml.h"
#include "aion/gameserver/model/templates/npc/fwd.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.GlobalDropData.
 * <p>
 * C++: processRules is post-processing of the unpublished holder (DataManager::init, static-data.md §3.3), so it is not const. Java passes
 * `NPC_DATA.getNpcData()` (HashMap values); the C++ caller passes NpcData::getNpcData() in the same order.
 *
 * @author AionCool, Bobobear, Neon
 */
class GlobalDropData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/GlobalDropData.xml.inc"
public:
	void processRules(const std::vector<const model::templates::npc::NpcTemplate*>& npcs);

private:
	static std::vector<model::templates::globaldrops::GlobalDropNpc> getAllowedNpcs(const model::templates::globaldrops::GlobalRule& rule,
	                                                                                const std::vector<const model::templates::npc::NpcTemplate*>& npcs);

public:
	int32_t size() const;
};

} // namespace aion::gameserver::dataholders
