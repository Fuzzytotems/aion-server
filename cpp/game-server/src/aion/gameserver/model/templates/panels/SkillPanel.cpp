#include "aion/gameserver/model/templates/panels/SkillPanel.h"

#include <string>

#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::model::templates::panels {

namespace {
const std::vector<int32_t>& panelSkills(const std::optional<std::vector<int32_t>>& skills, int32_t panelId) {
	if (!skills)
		throw runtime::NullPointerException("SkillPanel " + std::to_string(panelId) + " has no panel skills");
	return *skills;
}
} // namespace

bool SkillPanel::canUseSkill(int32_t skillId, int32_t level) const {
	for (int32_t skill : panelSkills(skills, id)) {
		if ((skill >> 8) == skillId && (skill & 0xFF) == level) {
			return true;
		}
	}
	return false;
}

bool SkillPanel::isSkillPresent(int32_t skillId) const {
	for (int32_t skill : panelSkills(skills, id)) {
		if ((skill >> 8) == skillId) {
			return true;
		}
	}
	return false;
}

} // namespace aion::gameserver::model::templates::panels
