#include "aion/gameserver/handlers/ai/instance/darkPoeta/CalindiFlamelordAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::handlers::ai::instance::darkPoeta {

AION_AI(CalindiFlamelordAI, "calindi_flamelord");

// Java CalindiFlamelordAI.java:27-31
void CalindiFlamelordAI::handleSpawned() {
	AggressiveNpcAI::handleSpawned();
	scheduleWipe();
}

// Java CalindiFlamelordAI.java:33-39
void CalindiFlamelordAI::scheduleWipe() {
	PacketSendUtility::broadcastToMap(getOwner(), SM_SYSTEM_MESSAGE::STR_MSG_INSTANCE_A_RANK_BATTLE_TIME());
	wipeTask = ThreadPoolManager::getInstance().schedule({this}, [this] {
		if (!isDead())
			getOwner().queueSkill(19679, 50, 3000);
	}, 600000);
}

// Java CalindiFlamelordAI.java:41-60
void CalindiFlamelordAI::onEndUseSkill(const skillengine::model::SkillTemplate* skillTemplate, int32_t skillLevel) {
	if (skillTemplate == nullptr) // Java: skillTemplate.getSkillId() on null
		throw runtime::NullPointerException("CalindiFlamelordAI: skill template");
	switch (skillTemplate->getSkillId()) {
		case 18233:
			if (getLifeStats()->getHpPercentage() >= 30 && getLifeStats()->getHpPercentage() <= 60) {
				spawn(281267, 1191.2714f, 1220.5795f, 144.2901f, int8_t{36});
				spawn(281267, 1188.3695f, 1257.1322f, 139.66028f, int8_t{80});
				spawn(281267, 1177.1423f, 1253.9136f, 140.58705f, int8_t{97});
				spawn(281267, 1163.5889f, 1231.9149f, 145.40042f, int8_t{118});
			} else {
				rndSpawnInRange(281268, 1, 2);
				rndSpawnInRange(281268, 1, 2);
			}
			break;
		case 19679: // You are unworthy.
			PacketSendUtility::broadcastToMap(getOwner(), SM_SYSTEM_MESSAGE::STR_MSG_INSTANCE_A_RANK_BATTLE_END());
			AIActions::deleteOwner(*this);
			break;
	}
}

// Java CalindiFlamelordAI.java:62-67
void CalindiFlamelordAI::handleDied() {
	runtime::FutureRef task = wipeTask.get();
	if (task != nullptr && !task->isCancelled())
		task->cancel(true);
	AggressiveNpcAI::handleDied();
}

} // namespace aion::gameserver::handlers::ai::instance::darkPoeta
