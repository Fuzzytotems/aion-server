#pragma once

#include <cstdint>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "aion/gameserver/dataholders/NpcShoutData.xml.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.NpcShoutData.
 * <p>
 * C++: the index holds `const NpcShout*` into the bound shout lists, which stay: Java empties and nulls the groups and lists while building it,
 * which would destroy the objects the index points to (ShoutList.h, docs/deviations/P4-07b.md). The visiting order (groups forward, lists and
 * npc ids backwards) and the counts equal Java's. Java's per-world LinkedHashMap order is not observable: the holder only looks npc ids up.
 * A Java null list is std::nullopt, a null type or pattern is std::nullopt.
 *
 * @author Rolandas
 */
class NpcShoutData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/NpcShoutData.xml.inc"
private:
	std::unordered_map<int32_t, std::unordered_map<int32_t, std::vector<const model::templates::npcshout::NpcShout*>>> shoutsByWorldNpcs;

	int32_t count = 0;

public:
	int32_t size() const;

	/**
	 * Get global npc shouts plus world specific shouts.
	 *
	 * @return std::nullopt (Java null) if not found
	 */
	std::optional<std::vector<const model::templates::npcshout::NpcShout*>> getNpcShouts(int32_t worldId, int32_t npcId) const;

	std::optional<std::vector<const model::templates::npcshout::NpcShout*>>
	getNpcShouts(int32_t worldId, int32_t npcId, std::optional<model::templates::npcshout::ShoutEventType> type) const;

	/**
	 * Gets shouts for npc
	 *
	 * @param worldId
	 *          - npc World Id
	 * @param npcId
	 *          - npc Id
	 * @param type
	 *          - shout event type
	 * @param pattern
	 *          - specific pattern; if null, returns all
	 * @param skillNo
	 *          - specific skill number; if 0, returns all
	 */
	std::optional<std::vector<const model::templates::npcshout::NpcShout*>> getNpcShouts(int32_t worldId, int32_t npcId,
	                                                                                     std::optional<model::templates::npcshout::ShoutEventType> type,
	                                                                                     std::optional<std::string_view> pattern,
	                                                                                     int32_t skillNo) const;
};

} // namespace aion::gameserver::dataholders
