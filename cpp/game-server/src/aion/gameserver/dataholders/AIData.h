#pragma once

#include <cstdint>
#include <unordered_map>

#include "aion/gameserver/dataholders/AIData.xml.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.AIData.
 * <p>
 * C++: the @XmlTransient index points into the bound `templates` storage, which stays after afterUnmarshal (static-data.md §2.6; Java sets the
 * list to null).
 *
 * @author xTz
 */
class AIData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/AIData.xml.inc"
private:
	std::unordered_map<int32_t, const model::templates::ai::AITemplate*> aiTemplate;

public:
	int32_t size() const;

	/** @return the ai template of the npc, nullptr (Java null) if there is none */
	const model::templates::ai::AITemplate* getAiTemplate(int32_t npcId) const;
};

} // namespace aion::gameserver::dataholders
