#include "aion/gameserver/restrictions/PlayerRestrictions.h"

#include <optional>
#include <string>

#include "aion/gameserver/GameServer.h"
#include "aion/gameserver/configs/main/GroupConfig.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/controllers/effect/PlayerEffectController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/PanelSkillsData.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/model/ActionState.h"
#include "aion/gameserver/model/ActionStateInfo.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/TransformModel.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/CustomPlayerState.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/player/PrivateStore.h"
#include "aion/gameserver/model/team/TeamTypeInfo.h"
#include "aion/gameserver/model/team/TemporaryPlayerTeam.h"
#include "aion/gameserver/model/team/alliance/PlayerAlliance.h"
#include "aion/gameserver/model/team/group/PlayerGroup.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/ItemUseLimits.h"
#include "aion/gameserver/model/templates/item/actions/ItemActions.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/model/templates/flypath/FlightPath.h"
#include "aion/gameserver/model/templates/panels/SkillPanel.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_RESPONSE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/AutoGroupService.h"
#include "aion/gameserver/services/VortexService.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"
#include "aion/gameserver/services/ban/ChatBanService.h"
#include "aion/gameserver/services/player/PlayerChatService.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/skillengine/model/SkillType.h"
#include "aion/gameserver/skillengine/model/TransformType.h"
#include "aion/gameserver/utils/ChatUtil.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::restrictions {

using model::gameobjects::Creature;
using model::gameobjects::VisibleObject;
using model::gameobjects::player::Player;
using model::templates::item::ItemTemplate;
using model::templates::panels::SkillPanel;
using network::aion::serverpackets::SM_ATTACK_RESPONSE;
using skillengine::effect::AbnormalState;
using skillengine::model::SkillTemplate;
using skillengine::model::SkillType;
using skillengine::model::TransformType;
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

// Java PlayerRestrictions.java:51-116 (m5b2-plan.md P-01; the AION_PARTIAL of M5b-1 C-01 is closed)
bool PlayerRestrictions::canUseSkill(Player& player, skillengine::model::Skill& skill) {
	if (player.isInPrison()) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_ACCUSE_TARGET_IS_NOT_VALID());
		return false;
	}
	// Java reads the target here, before any check: the resurrect arm at the end judges this target, not a later one
	runtime::Ptr<VisibleObject> target = player.getTarget();
	const SkillTemplate* template_ = skill.getSkillTemplate();

	if (!checkFly(player) || player.getLifeStats()->isAboutToDie() || player.isDead()) {
		return false;
	}

	if (player.getStore()) { // You cannot do that while you are running a Private Store.
		// Java ActionState.PERSONAL_SHOP.getL10n(), spelled out at the call site as checkFly does (ActionStateInfo.h)
		PacketSendUtility::sendPacket(player,
			SM_SYSTEM_MESSAGE::STR_SKILL_CANT_CAST(utils::ChatUtil::l10n(getL10nId(model::ActionState::PERSONAL_SHOP))));
		return false;
	}
	// item casts are interruptible (PlayerController cancels them), skill casts are not
	// java-race: isCasting() and getCastingSkill() read the field twice; a cast ending in between throws NullPointerException here as in Java
	if (player.isCasting() && player.getCastingSkill()->getItemTemplate() == nullptr)
		return false;

	if (!player.canAttack() && !template_->hasEvadeEffect()) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_SKILL_CAN_NOT_ATTACK_WHILE_IN_ABNORMAL_STATE());
		return false;
	}

	// in 3.0 players can use remove shock even when silenced
	if (template_->getType() == SkillType::MAGICAL && player.getEffectController()->isAbnormalSet(AbnormalState::SILENCE) &&
		!template_->hasEvadeEffect()) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_SKILL_CANT_CAST_MAGIC_SKILL_WHILE_SILENCED());
		return false;
	}

	if (template_->getType() == SkillType::PHYSICAL && player.getEffectController()->isAbnormalSet(AbnormalState::BIND)) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_SKILL_CANT_CAST_PHYSICAL_SKILL_IN_FEAR());
		return false;
	}

	if (player.isSkillDisabled(template_))
		return false;

	// cannot use skills while transformed
	if (player.getTransformModel().isActive()) {
		if (player.getTransformModel().cantUseSkills()) {
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_SKILL_CAN_NOT_CAST_IN_SHAPECHANGE());
			return false;
		}
		// can use only panel skills in FORM1
		if (player.getTransformModel().getType() == TransformType::FORM1) {
			const SkillPanel* panel = dataholders::DataManager::PANEL_SKILL_DATA->getSkillPanel(player.getTransformModel().getPanelId());
			if (panel == nullptr || !panel->isSkillPresent(skill.getSkillId())) {
				utils::audit::AuditLogger::log(player, "tried to use non panel skill while transformed in TransformType.FORM1");
				return false;
			}
		}
	}
	if (template_->hasResurrectEffect()) {
		runtime::Ptr<Player> targetPlayer = runtime::as<Player>(target); // Java: `target instanceof Player targetPlayer`, false for null
		if (!targetPlayer) {
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_SKILL_TARGET_IS_NOT_VALID());
			return false;
		}
		if (!targetPlayer->isDead()) {
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_SKILL_TARGET_IS_NOT_VALID());
			return false;
		}
	}
	return true;
}

// Java PlayerRestrictions.java:118-120
bool PlayerRestrictions::canInviteToGroup(Player& player, Player& target) {
	return canInviteToTeam(player, runtime::Ptr<Player>(target), false, player.getPlayerGroup());
}

// Java PlayerRestrictions.java:122-124
bool PlayerRestrictions::canInviteToAlliance(Player& player, Player& target) {
	return canInviteToTeam(player, runtime::Ptr<Player>(target), true, player.getPlayerAlliance());
}

// Java PlayerRestrictions.java:126-205
bool PlayerRestrictions::canInviteToTeam(Player& player, runtime::Ptr<Player> target, bool isAlliance,
	runtime::Ptr<model::team::TemporaryPlayerTeam> team) {
	using model::gameobjects::player::CustomPlayerState;
	if (player.isDead()) {
		PacketSendUtility::sendPacket(player, isAlliance ? SM_SYSTEM_MESSAGE::STR_FORCE_CANT_INVITE_WHEN_DEAD() : SM_SYSTEM_MESSAGE::STR_PARTY_CANT_INVITE_WHEN_DEAD());
		return false;
	}
	if (player.isInPrison()) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_INSTANCE_CANT_INVITE_PARTY_COMMAND());
		return false;
	}
	if (!target) {
		PacketSendUtility::sendPacket(player, isAlliance ? SM_SYSTEM_MESSAGE::STR_FORCE_NO_USER_TO_INVITE() : SM_SYSTEM_MESSAGE::STR_PARTY_NO_USER_TO_INVITE());
		return false;
	}
	if ((target->isInCustomState(CustomPlayerState::ENEMY_OF_ALL_PLAYERS) && !target->isInFfaTeamMode()) ||
		(player.isInCustomState(CustomPlayerState::ENEMY_OF_ALL_PLAYERS) && !player.isInFfaTeamMode())) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_DISABLE("FFA mode"));
		return false;
	}
	if (services::AutoGroupService::getInstance().isInAutoInstance(player) || services::AutoGroupService::getInstance().isInAutoInstance(*target)) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_INSTANCE_CANT_INVITE_PARTY_COMMAND());
		return false;
	}
	if (team) {
		runtime::Ptr<model::team::alliance::PlayerAlliance> alliance = runtime::as<model::team::alliance::PlayerAlliance>(team);
		if (!team->isLeader(player) && (!alliance || !alliance->isViceCaptain(player))) {
			PacketSendUtility::sendPacket(player,
				isAlliance ? SM_SYSTEM_MESSAGE::STR_FORCE_ONLY_LEADER_CAN_INVITE() : SM_SYSTEM_MESSAGE::STR_PARTY_ONLY_LEADER_CAN_INVITE());
			return false;
		}
		if (team->isFull()) {
			PacketSendUtility::sendPacket(player, isAlliance ? SM_SYSTEM_MESSAGE::STR_FORCE_CANT_ADD_NEW_MEMBER() : SM_SYSTEM_MESSAGE::STR_PARTY_CANT_ADD_NEW_MEMBER());
			return false;
		}
	}
	if (target->equals(player)) {
		PacketSendUtility::sendPacket(player, isAlliance ? SM_SYSTEM_MESSAGE::STR_FORCE_CAN_NOT_INVITE_SELF() : SM_SYSTEM_MESSAGE::STR_PARTY_CAN_NOT_INVITE_SELF());
		return false;
	}
	if (target->getRace() != player.getRace() &&
		(isAlliance ? !configs::main::GroupConfig::ALLIANCE_INVITEOTHERFACTION.load() : !configs::main::GroupConfig::GROUP_INVITEOTHERFACTION.load())) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_PARTY_CANT_INVITE_OTHER_RACE());
		return false;
	}
	if (target->isDead()) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_UI_PARTY_DEAD());
		return false;
	}
	runtime::Ptr<model::team::TemporaryPlayerTeam> targetTeam = target->getCurrentTeam();
	if (targetTeam) {
		if (targetTeam.rawPointer() == team.rawPointer()) { // Java: targetTeam == team (identity)
			PacketSendUtility::sendPacket(player, isAlliance ? SM_SYSTEM_MESSAGE::STR_FORCE_HE_IS_ALREADY_MEMBER_OF_OUR_FORCE(target->getName())
															 : SM_SYSTEM_MESSAGE::STR_PARTY_HE_IS_ALREADY_MEMBER_OF_OUR_PARTY(target->getName()));
			return false;
		}
		runtime::Ptr<model::team::group::PlayerGroup> targetGroup = runtime::as<model::team::group::PlayerGroup>(targetTeam);
		if (isAlliance && targetGroup) {
			if (team && targetGroup->size() + team->size() > team->getMaxMemberCount()) {
				PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_FORCE_INVITE_FAILED_NOT_ENOUGH_SLOT());
				return false;
			}
		} else {
			PacketSendUtility::sendPacket(player, runtime::as<model::team::alliance::PlayerAlliance>(targetTeam)
													  ? SM_SYSTEM_MESSAGE::STR_FORCE_ALREADY_OTHER_FORCE(target->getName())
													  : SM_SYSTEM_MESSAGE::STR_PARTY_HE_IS_ALREADY_MEMBER_OF_OTHER_PARTY(target->getName()));
			return false;
		}
	}
	runtime::Ptr<model::team::alliance::PlayerAlliance> alliance = runtime::as<model::team::alliance::PlayerAlliance>(team);
	if (alliance && model::team::isDefence(alliance->getTeamType())) {
		if (targetTeam) {
			for (const runtime::Ptr<model::gameobjects::AionObject>& object : targetTeam->getMembers()) {
				Player& tm = *runtime::cast<Player>(object);
				if (tm.isInInstance()) {
					PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_FORCE_CANT_INVITE_WHEN_HE_IS_IN_INSTANCE());
					return false;
				} else if (!services::VortexService::getInstance().isInsideVortexZone(tm)) {
					// TODO: chk on retail
					PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_PARTY_ALLIANCE_CANT_INVITE_WHEN_HE_IS_ASKED_QUESTION(tm.getName()));
					return false;
				}
			}
		} else if (!services::VortexService::getInstance().isInsideVortexZone(*target)) {
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_CANNOT_INVITE_DEFENSE_FORCE());
			return false;
		}
	}
	return true;
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

// Java PlayerRestrictions.java:240-252 (m5b3-plan.md P-01, optional: every callee is ported; its callers, the exchange and private store
// packets, are M5c's)
bool PlayerRestrictions::canTrade(runtime::Ptr<Player> player) {
	if (!player || player->isDead() || !player->isOnline())
		return false;
	if (GameServer::isShuttingDownSoon()) {
		PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_MSG_DISABLE("Shutdown Progress"));
		return false;
	}
	if (player->isTrading()) {
		PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_EXCHANGE_PARTNER_IS_EXCHANGING_WITH_OTHER());
		return false;
	}
	return true;
}

// Java PlayerRestrictions.java:254-275
bool PlayerRestrictions::canChat(runtime::Ptr<Player> player) {
	if (!player || !player->isOnline())
		return false;

	if (player->isInPrison()) {
		utils::PacketSendUtility::sendPacket(*player,
			network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_INGAME_BLOCK_IN_NO_CHAT(player->getPrisonDurationSeconds() / 60 + 1));
		return false;
	}

	if (services::ban::ChatBanService::isBanned(*player)) {
		utils::PacketSendUtility::sendPacket(*player,
			network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_INGAME_BLOCK_IN_NO_CHAT(services::ban::ChatBanService::getBanMinutes(*player)));
		return false;
	}

	if (services::player::PlayerChatService::isFlooding(*player)) {
		services::ban::ChatBanService::banPlayer(*player, 2 * 60 * 1000);
		utils::PacketSendUtility::sendPacket(*player, network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_FLOODING());
		return false;
	}

	return true;
}

// Java PlayerRestrictions.java:277-369 (m5b3-plan.md P-01): the restriction step of CM_USE_ITEM, after the item-use observers were notified
bool PlayerRestrictions::canUseItem(runtime::Ptr<Player> player, model::gameobjects::Item& item) {
	if (!player || !player->isOnline())
		return false;

	if (player->isInPrison()) {
		PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_MSG_ACCUSE_TARGET_IS_NOT_VALID());
		return false;
	}

	if (player->getLifeStats()->isAboutToDie() || player->isDead())
		return false;

	if (player->getEffectController()->isInAnyAbnormalState(AbnormalState::CANT_ATTACK_STATE)) {
		PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_SKILL_CAN_NOT_USE_ITEM_WHILE_IN_ABNORMAL_STATE());
		return false;
	}

	// cannot use item while transformed
	if (player->getTransformModel().cantUseItems()) {
		// client sends message by itself
		return false;
	}

	if (player->getStore()) { // You cannot use an item while running a Private Store.
		// Java ActionState.PERSONAL_SHOP.getL10n(), spelled out at the call site as checkFly does (ActionStateInfo.h)
		PacketSendUtility::sendPacket(*player,
			SM_SYSTEM_MESSAGE::STR_MSG_CANNOT_USE_ITEM_DURING_PATH_FLYING(utils::ChatUtil::l10n(getL10nId(model::ActionState::PERSONAL_SHOP))));
		return false;
	}

	// Prevents potion spamming, and relogging to use kisks/aether jelly/long CD items.
	if (player->hasCooldown(item)) {
		PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_ITEM_CANT_USE_UNTIL_DELAY_TIME());
		return false;
	}

	const ItemTemplate* itemTemplate = item.getItemTemplate();
	// Checked before the "no actions" fallback below so a race mismatch reports correctly even without one
	if (itemTemplate->getRace() != model::Race::PC_ALL && itemTemplate->getRace() != player->getRace()) {
		PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_CANNOT_USE_ITEM_INVALID_RACE());
		return false;
	}

	// getItemActions() answers the bound list, empty where Java's field is null - Java's getter answers Collections.emptyList() there
	// (ItemActions.java:38-40, ItemActions.h)
	const model::templates::item::actions::ItemActions* itemActions = itemTemplate->getActions();
	if (itemActions == nullptr || itemActions->getItemActions().empty()) {
		if (!questEngine::QuestEngine::getInstance().isRegisteredQuestItem(item.getItemId())) {
			PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_ITEM_IS_NOT_USABLE());
			return false;
		}
	}

	const model::templates::item::ItemUseLimits* limits = itemTemplate->getUseLimits();
	if (limits->getGenderPermitted() && *limits->getGenderPermitted() != player->getGender()) {
		PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_CANNOT_USE_ITEM_INVALID_GENDER());
		return false;
	}

	if (!itemTemplate->isClassSpecific(player->getCommonData()->getPlayerClass())) {
		PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_CANNOT_USE_ITEM_INVALID_CLASS());
		return false;
	}

	int32_t requiredLevel = itemTemplate->getRequiredLevel(player->getPlayerClass());
	if (requiredLevel > player->getLevel()) {
		PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_CANNOT_USE_ITEM_TOO_LOW_LEVEL_MUST_BE_THIS_LEVEL(item.getL10n(), requiredLevel));
		return false;
	}

	int8_t levelRestrict = itemTemplate->getMaxLevelRestrict(player->getPlayerClass());
	if (levelRestrict != 0 && player->getLevel() > levelRestrict) {
		PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_CANNOT_USE_ITEM_TOO_HIGH_LEVEL(levelRestrict, item.getL10n()));
		return false;
	}

	if (itemTemplate->hasAreaRestriction()) {
		const world::zone::ZoneName* restriction = itemTemplate->getUseArea();
		if (!player->isInsideItemUseZone(restriction)) {
			PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_SKILL_CAN_NOT_USE_ITEM_IN_CURRENT_POSITION());
			return false;
		}
	}

	if (std::optional<model::Race> activationRace = itemTemplate->getActivationRace()) {
		// TODO: check retail messages
		if (!runtime::as<Creature>(player->getTarget())) {
			PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_ITEM_CANT_FIND_VALID_TARGET());
			return false;
		}
		// java-race: Java reads player.getTarget() a second time for the cast; a target changed in between to null or to a non-Creature throws
		// NullPointerException or ClassCastException here as in Java (runtime::cast and the Ptr dereference)
		if (runtime::cast<Creature>(player->getTarget())->getRace() != *activationRace) {
			PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_SKILL_CANT_CAST_TO_CURRENT_TARGET());
			return false;
		}
	}

	return true;
}

// Java PlayerRestrictions.java:371-389 (m5b3-plan.md P-01): the first check of CM_EQUIP_ITEM after cancelUseItem
bool PlayerRestrictions::canChangeEquip(Player& player) {
	if (player.isInPrison()) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_ACCUSE_TARGET_IS_NOT_VALID());
		return false;
	}
	if (player.getController().isUnderStance()) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_SKILL_CAN_NOT_EQUIP_ITEM_WHILE_IN_CURRENT_STANCE());
		return false;
	}
	if (player.getEffectController()->isInAnyAbnormalState(AbnormalState::CANT_ATTACK_STATE)) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_SKILL_CAN_NOT_EQUIP_ITEM_WHILE_IN_ABNORMAL_STATE());
		return false;
	}
	if (player.getController().hasScheduledTask(model::TaskId::ITEM_USE)) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_CANT_EQUIP_ITEM_IN_ACTION());
		return false;
	}
	return true;
}

} // namespace aion::gameserver::restrictions
