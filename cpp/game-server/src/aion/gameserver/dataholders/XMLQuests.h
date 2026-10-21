#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

#include "aion/gameserver/dataholders/XMLQuests.xml.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.XMLQuests.
 * <p>
 * C++: the index points into the bound `data` storage, which stays after afterUnmarshal (static-data.md §2.6). getAllQuests returns the quests
 * in Java's HashMap<Integer, XMLQuest> iteration order.
 * <p>
 * Java's setData is replaced: //reload publishes a new holder from the files (DataManager::XML_QUESTS.republish in Reload.cpp) instead of
 * setting the quests of the published one, which stays immutable.
 *
 * @author MrPoke
 */
class XMLQuests : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/XMLQuests.xml.inc"
private:
	std::unordered_map<int32_t, const questEngine::handlers::models::XMLQuest*> questsById;
	/** C++ only: questsById.values() in Java's HashMap iteration order */
	std::vector<const questEngine::handlers::models::XMLQuest*> questsInHashOrder;

public:
	/** Java `questsById.values()` (HashMap iteration order) */
	const std::vector<const questEngine::handlers::models::XMLQuest*>& getAllQuests() const;

	/** @return the quest, nullptr (Java null) if there is none */
	const questEngine::handlers::models::XMLQuest* getQuest(int32_t questId) const;

	/** Java: replaces the quests and runs afterUnmarshal again (//reload only; deferred, design D3) */
};

} // namespace aion::gameserver::dataholders
