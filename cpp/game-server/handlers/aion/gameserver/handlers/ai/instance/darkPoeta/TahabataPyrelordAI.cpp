#include "aion/gameserver/handlers/ai/instance/darkPoeta/TahabataPyrelordAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::handlers::ai::instance::darkPoeta {

AION_AI(TahabataPyrelordAI, "tahabata_pyrelord");

// Java TahabataPyrelordAI.java:27-31
void TahabataPyrelordAI::handleSpawned() {
	AggressiveNpcAI::handleSpawned();
	scheduleWipe();
}

// Java TahabataPyrelordAI.java:33-39
void TahabataPyrelordAI::scheduleWipe() {
	PacketSendUtility::broadcastToMap(getOwner(), SM_SYSTEM_MESSAGE::STR_MSG_INSTANCE_S_RANK_BATTLE_TIME());
	wipeTask = ThreadPoolManager::getInstance().schedule({this}, [this] {
		if (!isDead())
			getOwner().queueSkill(19679, 50, 3000);
	}, 300000);
}

// Java TahabataPyrelordAI.java:41-61
void TahabataPyrelordAI::onEndUseSkill(const skillengine::model::SkillTemplate* skillTemplate, int32_t skillLevel) {
	if (skillTemplate == nullptr) // Java: skillTemplate.getSkillId() on null
		throw runtime::NullPointerException("TahabataPyrelordAI: skill template");
	switch (skillTemplate->getSkillId()) {
		case 19679: // You are unworthy.
			PacketSendUtility::broadcastToMap(getOwner(), SM_SYSTEM_MESSAGE::STR_MSG_INSTANCE_S_RANK_BATTLE_END());
			AIActions::deleteOwner(*this);
			break;
		case 18236:
			spawn(281258, 1191.2714f, 1220.5795f, 144.2901f, int8_t{36});
			spawn(281258, 1188.3695f, 1257.1322f, 139.66028f, int8_t{80});
			spawn(281258, 1177.1423f, 1253.9136f, 140.58705f, int8_t{97});
			spawn(281258, 1163.5889f, 1231.9149f, 145.40042f, int8_t{118});
			break;
		case 18241:
			spawn(281259, 1182.0021f, 1244.0125f, 142.67587f, int8_t{88});
			spawn(281259, 1192.3885f, 1236.5231f, 142.50638f, int8_t{68});
			spawn(281259, 1185.647f, 1227.2747f, 144.2261f, int8_t{32});
			spawn(281259, 1172.3302f, 1232.5709f, 144.70761f, int8_t{12});
			break;
	}
}

// Java TahabataPyrelordAI.java:63-66
void TahabataPyrelordAI::cancelTask() {
	runtime::FutureRef task = wipeTask.get();
	if (task != nullptr && !task->isCancelled())
		task->cancel(true);
}

// Java TahabataPyrelordAI.java:68-72
void TahabataPyrelordAI::handleDied() {
	cancelTask();
	AggressiveNpcAI::handleDied();
}

// Java TahabataPyrelordAI.java:74-78
void TahabataPyrelordAI::handleDespawned() {
	AggressiveNpcAI::handleDespawned();
	cancelTask();
}

} // namespace aion::gameserver::handlers::ai::instance::darkPoeta
