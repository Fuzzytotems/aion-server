#include "aion/gameserver/handlers/ai/instance/dragonLordsRefuge/EmpyreanLordAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/ai/manager/EmoteManager.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge {

AION_AI(EmpyreanLordAI, "empyrean_lord");

// Java EmpyreanLordAI.java:30-61
void EmpyreanLordAI::handleSpawned() {
	GeneralNpcAI::handleSpawned();
	ThreadPoolManager::getInstance().schedule({this}, [this] {
		getPosition()->getWorldMapInstance()->forEachNpc([](Npc& npc) {
			if (npc.getNpcId() >= 219532 && npc.getNpcId() <= 219539)
				npc.getController().die();
		});
	}, 9000);

	tiamatId = getPosition()->getMapId() == 300520000 ? 219361 : 236276;
	switch (getNpcId()) {
		case 219488:
		case 856020:
			ThreadPoolManager::getInstance().schedule({this}, [this] { AIActions::useSkill(*this, 20932); }, 8500);
			break;
		case 219491:
		case 856023:
			ThreadPoolManager::getInstance().schedule({this}, [this] { AIActions::useSkill(*this, 20936); }, 8500);
			break;
		case 219489:
		case 856021:
			AIActions::targetCreature(*this, *getPosition()->getWorldMapInstance()->getNpc(tiamatId.get()));
			ThreadPoolManager::getInstance().schedule({this}, [this] { AIActions::useSkill(*this, 20929); }, 8500);
			break;
		case 219492:
		case 856024:
			AIActions::targetCreature(*this, *getPosition()->getWorldMapInstance()->getNpc(tiamatId.get()));
			ThreadPoolManager::getInstance().schedule({this}, [this] { AIActions::useSkill(*this, 20933); }, 2500);
			break;
	}
}

// Java EmpyreanLordAI.java:63-75
void EmpyreanLordAI::onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) {
	switch (skillTemplate->getSkillId()) {
		case 20932:
		case 20936: {
			runtime::Ptr<Npc> tiamat = getPosition()->getWorldMapInstance()->getNpc(tiamatId.get());
			AIActions::targetCreature(*this, *tiamat);
			getAggroList().addHate(*tiamat, std::numeric_limits<int32_t>::max() / 4);
			setStateIfNot(gameserver::ai::AIState::FIGHT);
			gameserver::ai::manager::EmoteManager::emoteStartAttacking(getOwner(), *tiamat);
			break;
		}
	}
}

// Java EmpyreanLordAI.java:77-81
void EmpyreanLordAI::handleAttack(runtime::Ptr<Creature> creature) {
	GeneralNpcAI::handleAttack(creature);
	hpPhases->tryEnterNextPhase(*this);
}

// Java EmpyreanLordAI.java:83-93
void EmpyreanLordAI::handleHpPhase(int32_t phaseHpPercent) {
	switch (getNpcId()) {
		case 219488:
		case 856020:
		case 219491:
		case 856023:
			switch (phaseHpPercent) {
				case 50:
					PacketSendUtility::broadcastToMap(getOwner(), SM_SYSTEM_MESSAGE::IDTIAMAT_TIAMAT_GOD_HP_LOWER_THAN_50p());
					break;
				case 15:
					PacketSendUtility::broadcastToMap(getOwner(), SM_SYSTEM_MESSAGE::IDTIAMAT_TIAMAT_GOD_HP_LOWER_THAN_15p());
					break;
			}
			break;
	}
}

} // namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge
