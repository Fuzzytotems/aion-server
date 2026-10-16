#include "aion/gameserver/handlers/ai/instance/theShugoEmperorsVault/IDSweep_Shugos.h"

#include <cmath>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/instance/handlers/InstanceHandler.h"
#include "aion/gameserver/model/instance/instancescore/InstanceScore.h"
#include "aion/gameserver/model/stats/container/NpcGameStats.h"
#include "aion/gameserver/model/templates/stats/StatsTemplate.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::theShugoEmperorsVault {

AION_AI(IDSweep_Shugos, "IDSweep_shugos");

// Java IDSweep_Shugos.java:26-39
void IDSweep_Shugos::handleSpawned() {
	AggressiveNoLootNpcAI::handleSpawned();
	runtime::Ptr<InstanceHandler> handler = getOwner().getPosition()->getWorldMapInstance()->getInstanceHandler();
	runtime::Ptr<InstanceScore> reward;
	if (handler != nullptr) {
		reward = handler->getInstanceScore();
		if (reward != nullptr) {
			if (reward->getInstanceProgressionType() == InstanceProgressionType::END_PROGRESS)
				getOwner().getController().delete_();
		}
	}
	baseDamage = getOwner().getGameStats()->getStatsTemplate()->getAttack();
}

// Java IDSweep_Shugos.java:41-48 (Math.round(float) is floor(x + 0.5f))
float IDSweep_Shugos::modifyOwnerDamage(float damage, Creature& /*effected*/, runtime::Ptr<Effect> effect) {
	if (effect == nullptr) {
		int32_t rndDamage = commons::utils::Rnd::get(-static_cast<int32_t>(std::floor(static_cast<float>(baseDamage.get()) * 0.2f + 0.5f)),
			static_cast<int32_t>(std::floor(static_cast<float>(baseDamage.get()) * 0.25f + 0.5f)));
		damage = static_cast<float>(baseDamage.get() + rndDamage);
	}
	return damage;
}

} // namespace aion::gameserver::handlers::ai::instance::theShugoEmperorsVault
