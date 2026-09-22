#include "aion/gameserver/restrictions/PlayerRestrictions.h"

#include <string>

#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/model/ActionState.h"
#include "aion/gameserver/model/ActionStateInfo.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/TransformModel.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/model/templates/flypath/FlightPath.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_RESPONSE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/utils/ChatUtil.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::restrictions {

using model::gameobjects::Creature;
using model::gameobjects::VisibleObject;
using model::gameobjects::player::Player;
using network::aion::serverpackets::SM_ATTACK_RESPONSE;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using utils::PacketSendUtility;

// Java PlayerRestrictions.java:42-49
bool PlayerRestrictions::checkFly(Player& player) {
	if (player.isUsingFlightTransporterOrWindstream()) {
		// Java ActionState.PATH_FLYING.getL10n(): ActionState implements L10n, and an enum cannot derive the C++ L10n class, so the default
		// method is spelled out at the call site (ActionStateInfo.h).
		PacketSendUtility::sendPacket(player,
			SM_SYSTEM_MESSAGE::STR_SKILL_CANT_CAST(utils::ChatUtil::l10n(getL10nId(model::ActionState::PATH_FLYING))));
		// Java `"tried to attack " + player.getTarget() + " while using " + player.getFlightPath().getType()`: String.valueOf of a null target
		// is "null", and getType() is the enum constant's name. getFlightPath() is non-null here, because
		// isUsingFlightTransporterOrWindstream() read the same field (Player.cpp:647-649) - Java dereferences it without a check too.
		runtime::Ptr<VisibleObject> target = player.getTarget();
		utils::audit::AuditLogger::log(player,
			"tried to attack " + (target ? target->toString() : std::string("null")) + " while using " +
				std::string(xml::enumName(player.getFlightPath()->getType())));
		return false;
	}
	return true;
}

// Java PlayerRestrictions.java:51-116
bool PlayerRestrictions::canUseSkill(Player& player, skillengine::model::Skill& skill) {
	// m5b-plan.md C-01: every branch of the Java body reads the skill engine (SkillTemplate.hasEvadeEffect, getType, hasResurrectEffect,
	// Player.getCastingSkill/isSkillDisabled, EffectController.isAbnormalSet), which is P5-02 and M5b-2. Answering "no" is the conservative
	// half of Java's two answers and follows D4's precedent (GeneralNpcAI.chooseSkillAttack is an AION_PARTIAL returning false). There is no
	// caller at M5b-1: PlayerController::useSkill still calls standins::playerRestrictionsCanUseSkill (ControllerStandIns.cpp:98), which
	// m5b-plan.md E-01b leaves standing until M5b-2. docs/deviations/P5-13.md records the divergence.
	AION_PARTIAL("PlayerRestrictions.canUseSkill refuses every skill: its checks need the skill engine (M5b-2, m5b-plan.md C-01)");
	return false;
}

bool PlayerRestrictions::canInviteToGroup(Player& player, Player& target) {
	AION_UNPORTED();
}

bool PlayerRestrictions::canInviteToAlliance(Player& player, Player& target) {
	AION_UNPORTED();
}

bool PlayerRestrictions::canInviteToTeam(Player& player, runtime::Ptr<Player> target, bool isAlliance,
	runtime::Ptr<model::team::TemporaryPlayerTeam> team) {
	AION_UNPORTED();
}

// Java PlayerRestrictions.java:206-238
bool PlayerRestrictions::canAttack(Player& player, VisibleObject& target) {
	if (player.isInPrison()) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_ACCUSE_TARGET_IS_NOT_VALID());
		return false;
	}

	if (!player.isSpawned() || player.getLifeStats()->isAboutToDie() || player.isDead())
		return false;

	if (!checkFly(player))
		return false;

	if (runtime::Ptr<Player> targetPlayer = runtime::as<Player>(target); targetPlayer && targetPlayer->isUsingFlightTransporterOrWindstream())
		return false;

	if (!player.canAttack()) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_SKILL_CAN_NOT_ATTACK_WHILE_IN_ABNORMAL_STATE());
		PacketSendUtility::sendPacket(player, SM_ATTACK_RESPONSE::STOP_WITHOUT_MESSAGE(player.getGameStats()->getAttackCounter()));
		return false;
	}

	runtime::Ptr<Creature> creature = runtime::as<Creature>(target);
	if (!creature || creature->isDead() || creature->getLifeStats()->isAboutToDie()) {
		PacketSendUtility::sendPacket(player, SM_ATTACK_RESPONSE::STOP_INVALID_TARGET(player.getGameStats()->getAttackCounter()));
		return false;
	}

	// cannot attack while transformed
	if (player.getTransformModel().cantAttack()) {
		return false;
	}

	return player.isEnemy(*creature);
}

bool PlayerRestrictions::canTrade(runtime::Ptr<Player> player) {
	AION_UNPORTED();
}

bool PlayerRestrictions::canChat(runtime::Ptr<Player> player) {
	AION_UNPORTED();
}

bool PlayerRestrictions::canUseItem(runtime::Ptr<Player> player, model::gameobjects::Item& item) {
	AION_UNPORTED();
}

bool PlayerRestrictions::canChangeEquip(Player& player) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::restrictions
