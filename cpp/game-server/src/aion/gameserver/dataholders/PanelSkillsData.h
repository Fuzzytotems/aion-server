#pragma once

#include <cstdint>
#include <unordered_map>

#include "aion/gameserver/dataholders/PanelSkillsData.xml.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.PanelSkillsData.
 * <p>
 * C++: the index points into the bound `templates` storage, which stays after afterUnmarshal (static-data.md §2.6).
 *
 * @author xTz
 */
class PanelSkillsData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/PanelSkillsData.xml.inc"
private:
	std::unordered_map<int32_t, const model::templates::panels::SkillPanel*> skillPanels;

public:
	/** @return the skill panel, nullptr (Java null) if there is none */
	const model::templates::panels::SkillPanel* getSkillPanel(int32_t id) const;

	int32_t size() const;
};

} // namespace aion::gameserver::dataholders
