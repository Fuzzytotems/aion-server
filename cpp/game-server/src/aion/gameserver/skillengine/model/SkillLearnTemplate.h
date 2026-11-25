#pragma once

#include "aion/gameserver/skillengine/model/SkillLearnTemplate.xml.h"

#include <cstdint>

namespace aion::gameserver::skillengine::model {

/**
 * Java com.aionemu.gameserver.skillengine.model.SkillLearnTemplate.
 * <p>
 * Skill learning since 4.8 is different than before: every level of a skill has its own skillId, and `getLearnSkill()` is the skillId of the
 * next lower level (nullopt for Java null). Since upstream 267ce6033 the skill level is the skillLevel attribute (the generated getSkillLevel).
 *
 * @author ATracer, Neon
 */
class SkillLearnTemplate : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/skillengine/model/SkillLearnTemplate.xml.inc"
public:
	bool isStigma() const { return stigma > 0; }

	bool isLinkedStigma() const { return stigma == 4; }
};

} // namespace aion::gameserver::skillengine::model
