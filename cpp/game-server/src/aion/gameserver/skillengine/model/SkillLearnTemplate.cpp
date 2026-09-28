#include "aion/gameserver/skillengine/model/SkillLearnTemplate.h"

#include <string>

#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"

namespace aion::gameserver::skillengine::model {

int32_t SkillLearnTemplate::getSkillLevel() const {
	// Java: DataManager.SKILL_DATA.getSkillTemplate(skillId).getLvl()
	const SkillTemplate* skillTemplate = dataholders::DataManager::SKILL_DATA->getSkillTemplate(skillId);
	if (skillTemplate == nullptr)
		throw runtime::NullPointerException("no skill template " + std::to_string(skillId));
	return skillTemplate->getLvl();
}

} // namespace aion::gameserver::skillengine::model
