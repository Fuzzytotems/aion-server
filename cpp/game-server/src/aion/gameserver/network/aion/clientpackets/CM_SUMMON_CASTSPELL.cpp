#include "aion/gameserver/network/aion/clientpackets/CM_SUMMON_CASTSPELL.h"

#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/controllers/CreatureController.h"
#include "aion/gameserver/controllers/SummonController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/PetSkillData.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Summon.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/summons/SkillOrder.h"
#include "aion/gameserver/model/templates/VisibleObjectTemplate.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.network.aion.clientpackets.CM_SUMMON_CASTSPELL");

using model::gameobjects::Creature;
using model::gameobjects::Summon;

CM_SUMMON_CASTSPELL::CM_SUMMON_CASTSPELL(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_SUMMON_CASTSPELL.java:36-43
void CM_SUMMON_CASTSPELL::readImpl() {
	summonObjId = readD();
	skillId = readUH();
	skillLvl = readUC();
	targetObjId = readD();
	unk = readD();
}

// Java CM_SUMMON_CASTSPELL.java:45-84
void CM_SUMMON_CASTSPELL::runImpl() {
	const runtime::Ptr<model::gameobjects::player::Player> player = getConnection()->getActivePlayer();

	const runtime::Ptr<Creature> summonOrMercenary = player->getSummonOrMercenary(summonObjId);
	if (!summonOrMercenary) {
		sendPacket(serverpackets::SM_SYSTEM_MESSAGE::STR_SKILL_NOT_NEED_PET());
		return;
	}
	if (const runtime::Ptr<Summon> summon = runtime::as<Summon>(summonOrMercenary); summon && !summon->isPet()) {
		sendPacket(serverpackets::SM_SYSTEM_MESSAGE::STR_SKILL_NOT_NEED_PET());
		return;
	}

	runtime::Ptr<Creature> target;
	if (targetObjId != summonOrMercenary->getObjectId()) {
		const runtime::Ptr<model::gameobjects::VisibleObject> obj = summonOrMercenary->getKnownList().getObject(targetObjId);
		if (const runtime::Ptr<Creature> creature = runtime::as<Creature>(obj)) {
			target = creature;
		} else { // null or not a creature (attack should be client restricted)
			if (obj) // may be null due to lags while the target runs out of sight
				utils::audit::AuditLogger::log(*player, "tried to cast a summon spell on a wrong target: " + obj->toString());
			return;
		}
	} else {
		target = summonOrMercenary;
	}

	if (const runtime::Ptr<Summon> summon = runtime::as<Summon>(summonOrMercenary)) {
		const runtime::Ref<model::summons::SkillOrder> order(summon->retrieveNextSkillOrder());
		if (order && order->getTarget()->equals(*target)) {
			if (order->getSkillId() != skillId || order->getSkillLevel() != skillLvl)
				log.warn(player->toString() + " used summon order with a different skill: skillId " + std::to_string(skillId) + "->" +
					std::to_string(order->getSkillId()) + "; skillLvl " + std::to_string(skillLvl) + "->" + std::to_string(order->getSkillLevel()) + ".");
			summon->getController().useSkill(*order);
		}
	} else {
		summonOrMercenary->setTarget(target);
		if (dataholders::DataManager::PET_SKILL_DATA->petHasSkill(summonOrMercenary->getObjectTemplate()->getTemplateId(), skillId))
			summonOrMercenary->getController().useSkill(skillId, skillLvl);
		else
			utils::audit::AuditLogger::log(*player, "tried to use invalid mercenary skill " + std::to_string(skillId));
	}
}

AION_CLIENT_PACKET(CM_SUMMON_CASTSPELL);

} // namespace aion::gameserver::network::aion::clientpackets
