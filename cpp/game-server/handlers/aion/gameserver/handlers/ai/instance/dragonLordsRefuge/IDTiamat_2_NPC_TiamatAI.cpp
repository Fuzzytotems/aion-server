#include "aion/gameserver/handlers/ai/instance/dragonLordsRefuge/IDTiamat_2_NPC_TiamatAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"

namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge {

AION_AI(IDTiamat_2_NPC_TiamatAI, "IDTiamat_2_NPC_Tiamat");

// Java IDTiamat_2_NPC_TiamatAI.java:21-25
void IDTiamat_2_NPC_TiamatAI::handleBeforeSpawned() {
	GeneralNpcAI::handleBeforeSpawned();
	getOwner().overrideNpcType(CreatureType::PEACE);
}

// Java IDTiamat_2_NPC_TiamatAI.java:27-33
void IDTiamat_2_NPC_TiamatAI::onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) {
	if (skillTemplate->getSkillId() == 20919) {
		spawn(getNpcId() + 1, 466.7468f, 514.5500f, 417.4044f, int8_t{0}); // Dragon Tiamat
		AIActions::deleteOwner(*this);
	}
}

} // namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge
