#include "aion/gameserver/model/templates/item/Stigma.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::templates::item {

void Stigma::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	if (gainSkillGroup2.empty()) // Java: gainSkillGroup2 == null (absent)
		gainSkillGroups = {gainSkillGroup1};
	else
		gainSkillGroups = {gainSkillGroup1, gainSkillGroup2};
}

const std::vector<const skillengine::model::SkillTemplate*>* Stigma::getGainSkillsByGroup(int32_t groupNo) const {
	if (groupNo > 0 && groupNo <= static_cast<int32_t>(gainSkillGroups.size())) {
		// Java: DataManager.SKILL_DATA.getSkillTemplatesByGroup(gainSkillGroups[groupNo - 1]); SkillData (P4-09) declares no such method yet
		AION_UNPORTED();
	} else
		return nullptr;
}

} // namespace aion::gameserver::model::templates::item
