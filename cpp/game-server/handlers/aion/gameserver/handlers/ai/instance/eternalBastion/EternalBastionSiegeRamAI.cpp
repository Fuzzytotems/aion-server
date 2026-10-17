#include "aion/gameserver/handlers/ai/instance/eternalBastion/EternalBastionSiegeRamAI.h"

#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"

namespace aion::gameserver::handlers::ai::instance::eternalBastion {

AION_AI(EternalBastionSiegeRamAI, "eternal_bastion_siege_ram");

// Java EternalBastionSiegeRamAI.java:19-25
void EternalBastionSiegeRamAI::handleCreatureAggro(Creature& creature) {
	if (runtime::as<Npc>(runtime::Ptr<Creature>(creature)) != nullptr) {
		getOwner().getAggroList().addHate(creature, 500000);
		SkillEngine::getInstance().getSkill(getOwner(), 20778, 1, getTarget())->useSkill();
	}
}

// Java EternalBastionSiegeRamAI.java:27-30
void EternalBastionSiegeRamAI::onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) {
	SkillEngine::getInstance().getSkill(getOwner(), 20778, 1, getTarget())->useSkill();
}

} // namespace aion::gameserver::handlers::ai::instance::eternalBastion
