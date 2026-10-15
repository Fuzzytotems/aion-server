#include "aion/gameserver/dataholders/PanelSkillsData.h"

namespace aion::gameserver::dataholders {

void PanelSkillsData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	for (const model::templates::panels::SkillPanel& panel : templates)
		skillPanels.insert_or_assign(panel.getPanelId(), &panel);
	// Java: templates = null (the C++ index points into the storage, which stays)
}

const model::templates::panels::SkillPanel* PanelSkillsData::getSkillPanel(int32_t id) const {
	auto it = skillPanels.find(id);
	return it != skillPanels.end() ? it->second : nullptr;
}

int32_t PanelSkillsData::size() const {
	return static_cast<int32_t>(skillPanels.size());
}

} // namespace aion::gameserver::dataholders
