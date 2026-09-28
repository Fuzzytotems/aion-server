#pragma once

#include "aion/gameserver/questEngine/handlers/models/XMLQuest.xml.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <unordered_set>
#include <vector>

#include "aion/gameserver/questEngine/fwd.h"

namespace aion::gameserver::questEngine::handlers::models {

/** Java com.aionemu.gameserver.questEngine.handlers.models.XMLQuest. @author MrPoke, Hilgert, Pad */
class XMLQuest : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/questEngine/handlers/models/XMLQuest.xml.inc"
public:
	/** Java register(QuestEngine): registers the quest handler of this XML quest */
	virtual void register_(QuestEngine& questEngine) const = 0;

	/** @return the other npc ids that are valid in place of npcId for this quest, std::nullopt for Java null (none) */
	virtual std::optional<std::unordered_set<int32_t>> getAlternativeNpcs(int32_t npcId) const = 0;

protected:
	/**
	 * C++ only: the test every getAlternativeNpcs repeats for one id list, Java
	 * `ids.size() > 1 && ids.contains(npcId) ? ids.stream().filter(id -> id != npcId).collect(Collectors.toSet()) : null`
	 */
	static std::optional<std::unordered_set<int32_t>> otherNpcIds(const std::vector<int32_t>& ids, int32_t npcId) {
		if (ids.size() <= 1 || std::ranges::find(ids, npcId) == ids.end())
			return std::nullopt;
		std::unordered_set<int32_t> others;
		for (int32_t id : ids) {
			if (id != npcId)
				others.insert(id);
		}
		return others;
	}

	/** C++ only: the same with Java's `ids != null` test first (nullopt for Java null) */
	static std::optional<std::unordered_set<int32_t>> otherNpcIds(const std::optional<std::vector<int32_t>>& ids, int32_t npcId) {
		return ids ? otherNpcIds(*ids, npcId) : std::nullopt;
	}
};

} // namespace aion::gameserver::questEngine::handlers::models
