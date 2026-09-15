#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "aion/gameserver/model/templates/item/Stigma.xml.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::model::templates::item {

/** Java com.aionemu.gameserver.model.templates.item.Stigma. @author ATracer, Neon */
class Stigma : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/item/Stigma.xml.inc"
public:
	/** @return gain_skill_group1 and, if present, gain_skill_group2 (empty before the hook ran; Java null) */
	const std::vector<std::string>& getGainSkillGroups() const { return gainSkillGroups; }

	/** @return the skill templates of the group (1-based), nullptr (Java null) for an invalid group number */
	const std::vector<const skillengine::model::SkillTemplate*>* getGainSkillsByGroup(int32_t groupNo) const;

private:
	/** Java @XmlTransient String[] gainSkillGroups, filled by afterUnmarshal */
	std::vector<std::string> gainSkillGroups;
};

} // namespace aion::gameserver::model::templates::item
