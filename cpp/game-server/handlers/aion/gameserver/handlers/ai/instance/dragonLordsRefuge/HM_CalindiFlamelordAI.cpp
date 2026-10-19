#include "aion/gameserver/handlers/ai/instance/dragonLordsRefuge/HM_CalindiFlamelordAI.h"

#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge {

AION_AI(HM_CalindiFlamelordAI, "IDTiamat_HM_calindi_flamelord");

// Java HM_CalindiFlamelordAI.java:19-23
void HM_CalindiFlamelordAI::startHallucinatoryVictoryEvent() {
	if (getPosition()->getWorldMapInstance()->getNpc(731629) == nullptr && getPosition()->getWorldMapInstance()->getNpc(731630) == nullptr)
		getOwner().queueSkill(21887, 1);
}

// Java HM_CalindiFlamelordAI.java:25-38
void HM_CalindiFlamelordAI::onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) {
	switch (skillTemplate->getSkillId()) {
		case 21887:
			spawn(731629, 482.21f, 458.06f, 427.42f, int8_t{98});
			spawn(731630, 482.21f, 571.16f, 427.42f, int8_t{22});
			rndSpawn(856298);
			break;
		case 21888: {
			runtime::Ptr<Creature> target = getRandomTarget();
			if (target != nullptr)
				spawn(856299, target->getX(), target->getY(), target->getZ(), int8_t{0});
		}
	}
}

// Java HM_CalindiFlamelordAI.java:40-44
void HM_CalindiFlamelordAI::blazeEngraving() {
	if (commons::utils::Rnd::chance() < 3 && getPosition()->getWorldMapInstance()->getNpc(856299) == nullptr)
		getOwner().queueSkill(21888, 60);
}

} // namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge
