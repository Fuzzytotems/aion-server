#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/model/templates/panels/SkillPanel.xml.h"

namespace aion::gameserver::model::templates::panels {

/** Java com.aionemu.gameserver.model.templates.panels.SkillPanel. @author xTz */
class SkillPanel : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/panels/SkillPanel.xml.inc"
public:
	int32_t getPanelId() const { return id; }

	/** Java returns null: the panel skills are only checked through canUseSkill and isSkillPresent */
	const std::vector<int32_t>* getSkills() const { return nullptr; }

	/** @throws NullPointerException (Java) for a panel without panel_skills */
	bool canUseSkill(int32_t skillId, int32_t level) const;

	/** @throws NullPointerException (Java) for a panel without panel_skills */
	bool isSkillPresent(int32_t skillId) const;
};

} // namespace aion::gameserver::model::templates::panels
