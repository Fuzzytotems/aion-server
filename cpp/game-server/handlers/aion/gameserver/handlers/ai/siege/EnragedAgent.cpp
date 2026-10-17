#include "aion/gameserver/handlers/ai/siege/EnragedAgent.h"

#include "aion/gameserver/configs/main/SiegeConfig.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/handlers/ai/siege/AbstractSiegeProtectorAI.h"
#include "aion/gameserver/model/gameobjects/siege/SiegeNpc.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/Skill.h"

namespace aion::gameserver::handlers::ai::siege {

AION_AI(EnragedAgent, "enraged_agent");

// Java EnragedAgent.java:30-38 (no super call, as Java)
void EnragedAgent::onEffectEnd(runtime::Ptr<Effect> effect) {
	switch (effect->getSkillId()) {
		case 18704:
			ThreadPoolManager::getInstance().schedule({this}, [this] {
				runtime::Ref<skillengine::model::Skill> skill =
					SkillEngine::getInstance().getSkill(getOwner(), 18705, 60, getAggroList().getTarget(AggroTarget::MOST_HATED));
				if (skill == nullptr) // Java: getSkill(...).useSkill() on null
					throw runtime::NullPointerException("EnragedAgent: skill 18705");
				skill->useSkill();
			}, 650);
			break;
		default:
			break;
	}
}

// Java EnragedAgent.java:40-44
void EnragedAgent::modifyOwnerStat(Stat2& stat) {
	if (stat.getStat() == StatEnum::MAXHP)
		stat.setBaseRate(SiegeConfig::FORTRESS_PROTECTOR_HEALTH_MULTIPLIER.load());
}

// Java EnragedAgent.java:46-50
void EnragedAgent::handleBackHome() {
	SummonerAI::handleBackHome();
	getAggroList().clear(); // make sure old damages aren't counted in stopSiege
}

// Java EnragedAgent.java:52-56 (Java's cast (SiegeNpc) getOwner())
void EnragedAgent::handleDied() {
	SummonerAI::handleDied();
	AbstractSiegeProtectorAI::stopSiege(*runtime::cast<SiegeNpc>(getOwner()));
}

} // namespace aion::gameserver::handlers::ai::siege
