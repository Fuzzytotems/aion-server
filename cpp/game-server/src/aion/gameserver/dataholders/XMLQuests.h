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
 * in Java's HashMap<Integer, XMLQuest> iteration order. setData serves only the //reload command (deferred, design D3).
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
	void setData(std::vector<std::unique_ptr<questEngine::handlers::models::XMLQuest>> data);
};

} // namespace aion::gameserver::dataholders
