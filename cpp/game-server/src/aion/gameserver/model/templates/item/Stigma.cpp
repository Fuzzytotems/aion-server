#include "aion/gameserver/model/templates/item/Stigma.h"

#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/SkillData.h"

namespace aion::gameserver::model::templates::item {

void Stigma::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	if (gainSkillGroup2.empty()) // Java: gainSkillGroup2 == null (absent)
		gainSkillGroups = {gainSkillGroup1};
	else
		gainSkillGroups = {gainSkillGroup1, gainSkillGroup2};
}

const std::vector<const skillengine::model::SkillTemplate*>* Stigma::getGainSkillsByGroup(int32_t groupNo) const {
	if (groupNo > 0 && groupNo <= static_cast<int32_t>(gainSkillGroups.size()))
		return dataholders::DataManager::SKILL_DATA->getSkillTemplatesByGroup(gainSkillGroups[static_cast<size_t>(groupNo - 1)]);
	else
		return nullptr;
}

} // namespace aion::gameserver::model::templates::item
