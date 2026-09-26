#pragma once

#include "aion/gameserver/skillengine/model/SkillLearnTemplate.xml.h"

#include <cstdint>

namespace aion::gameserver::skillengine::model {

/**
 * Java com.aionemu.gameserver.skillengine.model.SkillLearnTemplate.
 * <p>
 * Skill learning since 4.8 is different than before: every level of a skill has its own skillId, and `getLearnSkill()` is the skillId of the
 * next lower level (nullopt for Java null).
 *
 * @author ATracer, Neon
 */
class SkillLearnTemplate : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/skillengine/model/SkillLearnTemplate.xml.inc"
public:
	/** @return the level of the learned skill template (Java: NullPointerException if there is no such template) */
	int32_t getSkillLevel() const;

	bool isStigma() const { return stigma > 0; }

	bool isLinkedStigma() const { return stigma == 4; }
};

} // namespace aion::gameserver::skillengine::model
