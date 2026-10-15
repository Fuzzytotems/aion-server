#include "aion/gameserver/handlers/ai/siege/BaseProtectorAI.h"

#include <optional>

#include "aion/gameserver/configs/main/SiegeConfig.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/controllers/attack/DamageInfo.h"
#include "aion/gameserver/controllers/attack/DamageList.h"
#include "aion/gameserver/controllers/attack/TeamDamageList.h"
#include "aion/gameserver/model/base/Base.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/team/TemporaryPlayerTeam.h"
#include "aion/gameserver/model/templates/spawns/basespawns/BaseSpawnTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/BaseService.h"

namespace aion::gameserver::handlers::ai::siege {

AION_AI(BaseProtectorAI, "base_protector");

// Java BaseProtectorAI.java:27-30
runtime::Ptr<model::templates::spawns::basespawns::BaseSpawnTemplate> BaseProtectorAI::getSpawnTemplate() {
	return runtime::cast<model::templates::spawns::basespawns::BaseSpawnTemplate>(AggressiveNpcAI::getSpawnTemplate());
}

// Java BaseProtectorAI.java:32-42
void BaseProtectorAI::handleDied() {
	AggressiveNpcAI::handleDied();
	runtime::Ptr<Base> base = services::BaseService::getInstance().getActiveBase(getSpawnTemplate()->getId());
	if (!base)
		return;
	std::optional<controllers::attack::DamageInfo> mostDamage = getAggroList().getFinalDamageList().toTeamDamages().getMostDamage();
	if (!mostDamage)
		throw runtime::NullPointerException("DamageInfo"); // Java: mostDamage.getAttacker() on null
	runtime::Ptr<AionObject> attacker = mostDamage->getAttacker();
	runtime::Ptr<Creature> bossKiller;
	if (runtime::Ptr<model::team::TemporaryPlayerTeam> team = runtime::as<model::team::TemporaryPlayerTeam>(attacker))
		bossKiller = team->getLeaderObject();
	else
		bossKiller = runtime::cast<Creature>(attacker);
	BaseOccupier newOccupier = base->getOccupier(bossKiller);
	services::BaseService::getInstance().capture(base->getId(), newOccupier);
}

// Java BaseProtectorAI.java:44-48
void BaseProtectorAI::modifyOwnerStat(Stat2& stat) {
	if (stat.getStat() == StatEnum::MAXHP && getOwner().getLevel() >= 65) // Avoid adjusting low-level zones
		stat.setBaseRate(SiegeConfig::BASE_PROTECTOR_HEALTH_MULTIPLIER.load());
}

// Java BaseProtectorAI.java:50-56
bool BaseProtectorAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::ALLOW_DECAY:
		case AIQuestion::ALLOW_RESPAWN:
		case AIQuestion::REWARD_LOOT:
		case AIQuestion::REMOVE_EFFECTS_ON_MAP_REGION_DEACTIVATE:
			return false;
		default:
			return AggressiveNpcAI::ask(question);
	}
}

} // namespace aion::gameserver::handlers::ai::siege
