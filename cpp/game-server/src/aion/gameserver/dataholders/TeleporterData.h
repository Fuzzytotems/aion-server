#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "aion/gameserver/dataholders/TeleporterData.xml.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.TeleporterData.
 * <p>
 * C++: the index points into the bound `templates` storage, which stays after afterUnmarshal (static-data.md §2.6).
 * getTeleporterTemplateByNpcId searches the templates in Java's HashMap<Integer, TeleporterTemplate> iteration order, so an npc listed by
 * several teleporters finds the same template.
 *
 * @author orz
 */
class TeleporterData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/TeleporterData.xml.inc"
private:
	std::unordered_map<int32_t, const model::templates::teleport::TeleporterTemplate*> teleporterTemplates;
	/** C++ only: teleporterTemplates.values() in Java's HashMap iteration order */
	std::vector<const model::templates::teleport::TeleporterTemplate*> templatesInHashOrder;

public:
	int32_t size() const;

	/** @return the first teleporter (Java HashMap order) of the npc, nullptr (Java null) if there is none */
	const model::templates::teleport::TeleporterTemplate* getTeleporterTemplateByNpcId(int32_t npcId) const;

	/** @return the teleporter, nullptr (Java null) if there is none */
	const model::templates::teleport::TeleporterTemplate* getTeleporterTemplateByTeleportId(int32_t teleportId) const;
};

} // namespace aion::gameserver::dataholders
