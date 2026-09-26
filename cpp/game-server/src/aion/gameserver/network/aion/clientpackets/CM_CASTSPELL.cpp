#include "aion/gameserver/network/aion/clientpackets/CM_CASTSPELL.h"

#include <string>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/PetSkillData.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/ActionState.h"
#include "aion/gameserver/model/ActionStateInfo.h"
#include "aion/gameserver/model/gameobjects/Summon.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/utils/ChatUtil.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

using model::gameobjects::player::Player;
using serverpackets::SM_SYSTEM_MESSAGE;

CM_CASTSPELL::CM_CASTSPELL(int32_t opcode, const StateSet& validStates)
	: AionClientPacket(opcode, validStates), receiveTime(commons::utils::currentTimeMillis()) {
}

// Java CM_CASTSPELL.java:36-71
void CM_CASTSPELL::readImpl() {
	spellid = readUH();
	level = readUC();

	targetType = readUC();

	switch (targetType) {
		case 0:
		case 3:
		case 4:
			targetObjectId = readD();
			break;
		case 1:
			x = readF();
			y = readF();
			z = readF();
			break;
		case 2:
			x = readF();
			y = readF();
			z = readF();
			readF(); // unk1
			readF(); // unk2
			readF(); // unk3
			readF(); // unk4
			readF(); // unk5
			readF(); // unk6
			readF(); // unk7
			readF(); // unk8
			break;
	}

	hitTime = readUH();
	unk = readD();
}

// Java CM_CASTSPELL.java:73-109
void CM_CASTSPELL::runImpl() {
	runtime::Ptr<Player> player = getConnection()->getActivePlayer();

	if (player->isDead()) {
		// Java ActionState.DEAD.getL10n(): ActionState implements L10n, which an enum cannot derive in C++, so the default method is spelled out
		// at the call site as PlayerRestrictions.checkFly does (ActionStateInfo.h)
		sendPacket(SM_SYSTEM_MESSAGE::STR_SKILL_CANT_CAST(utils::ChatUtil::l10n(getL10nId(model::ActionState::DEAD))));
		return;
	}

	if (spellid == 0) {
		player->getController().cancelCurrentSkill(nullptr);
		return;
	}
	if (dataholders::DataManager::PET_SKILL_DATA->isPetOrderSkill(spellid) && (!player->getSummon() || !player->getSummon()->isPet())) {
		sendPacket(SM_SYSTEM_MESSAGE::STR_SKILL_NOT_NEED_PET());
		return;
	}

	const skillengine::model::SkillTemplate* template_ = dataholders::DataManager::SKILL_DATA->getSkillTemplate(spellid);
	if (template_ == nullptr || template_->isPassive())
		return;

	if (player->isProtectionActive())
		player->getController().stopProtectionActiveTask();
	player->getController().cancelUseItem();

	if (player->getNextSkillUse() > receiveTime) {
		// lastSkill cannot be null, as nextSkillUse is zero on the first cast
		const skillengine::model::SkillTemplate* lastSkill = player->getLastSkill();
		if (lastSkill == nullptr) // Java: player.getLastSkill().getSkillId() on null
			throw runtime::NullPointerException("CM_CASTSPELL: " + player->toString() + " has a next skill use time but no last skill");
		int32_t lastSkillId = lastSkill->getSkillId();
		utils::audit::AuditLogger::log(*player, "tried to use skill " + std::to_string(spellid) + " " +
			std::to_string(player->getNextSkillUse() - receiveTime) + " ms too early. Previous skill: " + std::to_string(lastSkillId));
		if (player->getNextSkillUse() > commons::utils::currentTimeMillis()) {
			sendPacket(SM_SYSTEM_MESSAGE::STR_SKILL_NOT_READY());
			return;
		}
	}

	player->getController().useSkill(template_, targetType, x, y, z, hitTime, level);
}

AION_CLIENT_PACKET(CM_CASTSPELL);

} // namespace aion::gameserver::network::aion::clientpackets
