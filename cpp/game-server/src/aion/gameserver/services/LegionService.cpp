#include "aion/gameserver/services/LegionService.h"

#include <chrono>
#include <regex>
#include <span>
#include <vector>
#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/dao/InventoryDAO.h"
#include "aion/gameserver/dao/ItemStoneListDAO.h"
#include "aion/gameserver/dao/LegionDAO.h"
#include "aion/gameserver/dao/LegionMemberDAO.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/items/storage/IStorage.h"
#include "aion/gameserver/model/items/storage/StorageType.h"
#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/model/team/legion/LegionEmblem.h"
#include "aion/gameserver/model/team/legion/LegionHistoryAction.h"
#include "aion/gameserver/model/team/legion/LegionMember.h"
#include "aion/gameserver/model/team/legion/LegionWarehouse.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/commons/configuration/transformers/PatternTransformer.h"
#include "aion/gameserver/configs/main/LegionConfig.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/DeniedStatus.h"
#include "aion/gameserver/model/gameobjects/player/PlayerSettings.h"
#include "aion/gameserver/model/team/legion/LegionPermissionsMaskInfo.h"
#include "aion/gameserver/model/team/legion/LegionRankInfo.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/ChallengeTaskService.h"
#include "aion/gameserver/services/NameRestrictionService.h"
#include "aion/gameserver/services/trade/PricesService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/model/DialogPageInfo.h"
#include "aion/gameserver/model/gameobjects/player/RequestResponseHandler.h"
#include "aion/gameserver/model/gameobjects/player/ResponseRequester.h"
#include "aion/gameserver/model/items/storage/StorageTypeInfo.h"
#include "aion/gameserver/model/team/legion/LegionHistoryActionInfo.h"
#include "aion/gameserver/model/team/legion/LegionHistoryEntry.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ICON_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_LEGION_ADD_MEMBER.h"
#include "aion/gameserver/network/aion/serverpackets/SM_LEGION_EDIT.h"
#include "aion/gameserver/network/aion/serverpackets/SM_LEGION_HISTORY.h"
#include "aion/gameserver/network/aion/serverpackets/SM_LEGION_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_LEGION_LEAVE_MEMBER.h"
#include "aion/gameserver/network/aion/serverpackets/SM_LEGION_MEMBERLIST.h"
#include "aion/gameserver/network/aion/serverpackets/SM_LEGION_SEND_EMBLEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_LEGION_SEND_EMBLEM_DATA.h"
#include "aion/gameserver/network/aion/serverpackets/SM_LEGION_UPDATE_EMBLEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_LEGION_UPDATE_MEMBER.h"
#include "aion/gameserver/network/aion/serverpackets/SM_LEGION_UPDATE_NICKNAME.h"
#include "aion/gameserver/network/aion/serverpackets/SM_LEGION_UPDATE_SELF_INTRO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_LEGION_UPDATE_TITLE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUESTION_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_WAREHOUSE_INFO.h"
#include "aion/gameserver/runtime/fields/Array.h"
#include "aion/gameserver/services/SiegeService.h"
#include "aion/gameserver/services/conquerorAndProtectorSystem/ConquerorAndProtectorService.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"
#include "aion/gameserver/utils/collections/FixedElementCountSplitList.h"
#include "aion/gameserver/utils/collections/ListPart.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/services/player/PlayerService.h"

namespace aion::gameserver::services {

using model::gameobjects::player::Player;
using model::team::legion::Legion;
using model::team::legion::LegionEmblem;
using model::team::legion::LegionEmblemType;
using model::team::legion::LegionHistoryAction;
using model::team::legion::LegionMember;
using model::team::legion::LegionPermissionsMask;
using model::team::legion::LegionRank;
using network::aion::serverpackets::SM_DIALOG_WINDOW;
using network::aion::serverpackets::SM_LEGION_ADD_MEMBER;
using network::aion::serverpackets::SM_LEGION_EDIT;
using network::aion::serverpackets::SM_LEGION_HISTORY;
using network::aion::serverpackets::SM_LEGION_INFO;
using network::aion::serverpackets::SM_LEGION_LEAVE_MEMBER;
using network::aion::serverpackets::SM_LEGION_MEMBERLIST;
using network::aion::serverpackets::SM_LEGION_SEND_EMBLEM;
using network::aion::serverpackets::SM_LEGION_SEND_EMBLEM_DATA;
using network::aion::serverpackets::SM_LEGION_UPDATE_EMBLEM;
using network::aion::serverpackets::SM_LEGION_UPDATE_MEMBER;
using network::aion::serverpackets::SM_LEGION_UPDATE_NICKNAME;
using network::aion::serverpackets::SM_LEGION_UPDATE_SELF_INTRO;
using network::aion::serverpackets::SM_LEGION_UPDATE_TITLE;
using network::aion::serverpackets::SM_QUESTION_WINDOW;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using network::aion::serverpackets::SM_WAREHOUSE_INFO;

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.LegionService");

// ---- the anonymous RequestResponseHandlers of LegionService (fieldmap callback structs, friends of LegionService) ----

// fieldmap-class: com.aionemu.gameserver.services.LegionService$1
/** Java: the anonymous RequestResponseHandler<Npc> of requestDisbandLegion */
class LegionService_DisbandResponseHandler final : public model::gameobjects::player::RequestResponseHandler {
	AION_MAKE_REF_FRIEND
public:
	static runtime::Ref<LegionService_DisbandResponseHandler> create(model::gameobjects::Npc& npc) {
		return runtime::makeRef<LegionService_DisbandResponseHandler>(npc);
	}

	void acceptRequest(runtime::Ptr<model::gameobjects::Creature> requesterValue, Player& responder) override {
		static_cast<void>(requesterValue);
		runtime::Ptr<Legion> legion = responder.getLegion();
		int32_t unixTime = static_cast<int32_t>((commons::utils::currentTimeMillis() / 1000) + configs::main::LegionConfig::LEGION_DISBAND_TIME.load());
		legion->setDisbandTime(unixTime);
		LegionService::getInstance().updateMembersOfDisbandLegion(*legion, unixTime);
	}

protected:
	explicit LegionService_DisbandResponseHandler(model::gameobjects::Npc& npc)
		: RequestResponseHandler(runtime::Ptr<model::gameobjects::Creature>(npc)) {}
	~LegionService_DisbandResponseHandler() override = default;
};

// fieldmap-class: com.aionemu.gameserver.services.LegionService$2
/** Java: the anonymous RequestResponseHandler<Player> of invitePlayerToLegion, capturing the inviter's legion */
class LegionService_InviteResponseHandler final : public model::gameobjects::player::RequestResponseHandler {
	AION_MAKE_REF_FRIEND
public:
	const runtime::Ref<Legion> legion; // captured local Legion legion

	static runtime::Ref<LegionService_InviteResponseHandler> create(Player& activePlayer, Legion& legionValue) {
		return runtime::makeRef<LegionService_InviteResponseHandler>(activePlayer, legionValue);
	}

	void acceptRequest(runtime::Ptr<model::gameobjects::Creature> requesterValue, Player& responder) override {
		LegionService::getInstance().addToLegion(*legion, responder, *runtime::cast<Player>(requesterValue));
	}

	void denyRequest(runtime::Ptr<model::gameobjects::Creature> requesterValue, Player& responder) override {
		utils::PacketSendUtility::sendPacket(*runtime::cast<Player>(requesterValue), SM_SYSTEM_MESSAGE::STR_GUILD_INVITE_HE_REJECTED_INVITATION(responder.getName()));
	}

protected:
	LegionService_InviteResponseHandler(Player& activePlayer, Legion& legionValue)
		: RequestResponseHandler(runtime::Ptr<model::gameobjects::Creature>(activePlayer)), legion(legionValue) {}
	~LegionService_InviteResponseHandler() override = default;
};

// fieldmap-class: com.aionemu.gameserver.services.LegionService$3
/** Java: the anonymous RequestResponseHandler<Player> of startBrigadeGeneralChangeProcess (its requester is the new brigade general) */
class LegionService_ChangeProcessResponseHandler final : public model::gameobjects::player::RequestResponseHandler {
	AION_MAKE_REF_FRIEND
public:
	static runtime::Ref<LegionService_ChangeProcessResponseHandler> create(Player& newLegionLeader) {
		return runtime::makeRef<LegionService_ChangeProcessResponseHandler>(newLegionLeader);
	}

	void acceptRequest(runtime::Ptr<model::gameobjects::Creature> newBrigadeGeneral, Player& responder) override {
		LegionService::getInstance().appointBrigadeGeneral(responder, *runtime::cast<Player>(newBrigadeGeneral));
	}

protected:
	explicit LegionService_ChangeProcessResponseHandler(Player& newLegionLeader)
		: RequestResponseHandler(runtime::Ptr<model::gameobjects::Creature>(newLegionLeader)) {}
	~LegionService_ChangeProcessResponseHandler() override = default;
};

// fieldmap-class: com.aionemu.gameserver.services.LegionService$4
/** Java: the anonymous RequestResponseHandler<Player> of appointBrigadeGeneral(Player, Player) */
class LegionService_AppointResponseHandler final : public model::gameobjects::player::RequestResponseHandler {
	AION_MAKE_REF_FRIEND
public:
	static runtime::Ref<LegionService_AppointResponseHandler> create(Player& activePlayer) {
		return runtime::makeRef<LegionService_AppointResponseHandler>(activePlayer);
	}

	void acceptRequest(runtime::Ptr<model::gameobjects::Creature> requesterValue, Player& responder) override {
		Player& requestingPlayer = *runtime::cast<Player>(requesterValue);
		if (!responder.isOnline()) {
			utils::PacketSendUtility::sendPacket(requestingPlayer, SM_SYSTEM_MESSAGE::STR_GUILD_CHANGE_MASTER_NO_SUCH_USER());
		} else if (!LegionService::getInstance().legionRestrictions->canAppointBrigadeGeneral(requestingPlayer, responder)) {
			utils::audit::AuditLogger::log(requestingPlayer, "possibly tried to exploit legion leadership transfer");
		} else {
			LegionService::getInstance().appointBrigadeGeneral(*responder.getLegionMember());
		}
	}

	void denyRequest(runtime::Ptr<model::gameobjects::Creature> requesterValue, Player& responder) override {
		utils::PacketSendUtility::sendPacket(*runtime::cast<Player>(requesterValue),
			SM_SYSTEM_MESSAGE::STR_GUILD_CHANGE_MASTER_HE_DECLINE_YOUR_OFFER(responder.getName()));
	}

protected:
	explicit LegionService_AppointResponseHandler(Player& activePlayer)
		: RequestResponseHandler(runtime::Ptr<model::gameobjects::Creature>(activePlayer)) {}
	~LegionService_AppointResponseHandler() override = default;
};

// fieldmap-class: com.aionemu.gameserver.services.LegionService$5
/** Java: the anonymous RequestResponseHandler<Npc> of recreateLegion */
class LegionService_RecreateResponseHandler final : public model::gameobjects::player::RequestResponseHandler {
	AION_MAKE_REF_FRIEND
public:
	static runtime::Ref<LegionService_RecreateResponseHandler> create(model::gameobjects::Npc& npc) {
		return runtime::makeRef<LegionService_RecreateResponseHandler>(npc);
	}

	void acceptRequest(runtime::Ptr<model::gameobjects::Creature> requesterValue, Player& responder) override {
		static_cast<void>(requesterValue);
		runtime::Ptr<Legion> legion = responder.getLegion();
		legion->setDisbandTime(0);
		utils::PacketSendUtility::broadcastToLegion(*legion, SM_LEGION_EDIT(0x07));
		LegionService::getInstance().updateMembersOfRecreateLegion(*legion);
	}

protected:
	explicit LegionService_RecreateResponseHandler(model::gameobjects::Npc& npc)
		: RequestResponseHandler(runtime::Ptr<model::gameobjects::Creature>(npc)) {}
	~LegionService_RecreateResponseHandler() override = default;
};

LegionService::LegionService() : legionRestrictions(LegionRestrictions::create()) {
}

LegionService::~LegionService() = default;

LegionService& LegionService::getInstance() {
	static LegionService instance; // Java SingletonHolder
	return instance;
}

runtime::Ref<LegionService::LegionRestrictions> LegionService::LegionRestrictions::create() {
	return runtime::makeRef<LegionRestrictions>();
}

bool LegionService::LegionRestrictions::canCreateLegion(model::gameobjects::player::Player& activePlayer, std::string_view legionName) {
	/* Some reasons why legions can' be created */
	if (!NameRestrictionService::isValidLegionName(legionName) || NameRestrictionService::isForbidden(legionName)) {
		utils::PacketSendUtility::sendPacket(activePlayer, SM_SYSTEM_MESSAGE::STR_GUILD_CREATE_INVALID_GUILD_NAME());
		return false;
	} // STR_GUILD_CREATE_TOO_FAR_FROM_CREATOR_NPC TODO
	else if (!isFreeName(legionName)) {
		utils::PacketSendUtility::sendPacket(activePlayer, SM_SYSTEM_MESSAGE::STR_GUILD_CREATE_SAME_GUILD_EXIST());
		return false;
	} else if (activePlayer.isLegionMember()) {
		utils::PacketSendUtility::sendPacket(activePlayer, SM_SYSTEM_MESSAGE::STR_GUILD_CREATE_ALREADY_BELONGS_TO_GUILD());
		return false;
	} else if (activePlayer.getInventory().getKinah() < configs::main::LegionConfig::LEGION_CREATE_REQUIRED_KINAH.load()) {
		utils::PacketSendUtility::sendPacket(activePlayer, SM_SYSTEM_MESSAGE::STR_GUILD_CREATE_NOT_ENOUGH_MONEY());
		return false;
	}
	return true;
}

bool LegionService::LegionRestrictions::canInvitePlayer(model::gameobjects::player::Player& activePlayer, runtime::Ptr<model::gameobjects::player::Player> targetPlayer) {
	runtime::Ptr<model::team::legion::Legion> legion = activePlayer.getLegion();
	if (!targetPlayer) {
		utils::PacketSendUtility::sendPacket(activePlayer, SM_SYSTEM_MESSAGE::STR_GUILD_INVITE_NO_USER_TO_INVITE());
		return false;
	} else if (targetPlayer->getPlayerSettings()->isInDeniedStatus(model::gameobjects::player::DeniedStatus::GUILD)) {
		utils::PacketSendUtility::sendPacket(activePlayer, SM_SYSTEM_MESSAGE::STR_MSG_REJECTED_INVITE_GUILD(targetPlayer->getName()));
		return false;
	} else if (activePlayer.isDead()) {
		utils::PacketSendUtility::sendPacket(activePlayer, SM_SYSTEM_MESSAGE::STR_GUILD_INVITE_CANT_INVITE_WHEN_DEAD());
		return false;
	} else if (activePlayer.equals(*targetPlayer)) {
		utils::PacketSendUtility::sendPacket(activePlayer, SM_SYSTEM_MESSAGE::STR_GUILD_INVITE_CAN_NOT_INVITE_SELF());
		return false;
	} else if (targetPlayer->isLegionMember()) {
		if (legion->isMember(targetPlayer->getObjectId())) {
			utils::PacketSendUtility::sendPacket(activePlayer, SM_SYSTEM_MESSAGE::STR_GUILD_INVITE_HE_IS_MY_GUILD_MEMBER(targetPlayer->getName()));
		} else {
			utils::PacketSendUtility::sendPacket(activePlayer, SM_SYSTEM_MESSAGE::STR_GUILD_INVITE_HE_IS_OTHER_GUILD_MEMBER(targetPlayer->getName()));
		}
		return false;
	} else if (!activePlayer.getLegionMember()->hasRights(model::team::legion::LegionPermissionsMask::INVITE)) {
		utils::PacketSendUtility::sendPacket(activePlayer, SM_SYSTEM_MESSAGE::STR_GUILD_INVITE_DONT_HAVE_RIGHT_TO_INVITE());
		return false;
	} else if (activePlayer.getRace() != targetPlayer->getRace() && !configs::main::LegionConfig::LEGION_INVITEOTHERFACTION.load()) {
		utils::PacketSendUtility::sendPacket(activePlayer, SM_SYSTEM_MESSAGE::STR_GUILD_INVITE_CAN_NOT_INVITE_OTHER_RACE());
		return false;
	}
	return true;
}

bool LegionService::LegionRestrictions::canKickPlayer(model::gameobjects::player::Player& player, std::string_view charName, runtime::Ptr<model::team::legion::LegionMember> legionMember) {
	runtime::Ptr<model::team::legion::Legion> legion = player.getLegion();
	if (!legion) {
		utils::PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_GUILD_BANISH_I_AM_NOT_BELONG_TO_GUILD());
		return false;
	} else if (!legionMember || !legion->isMember(legionMember->getObjectId())) {
		utils::PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_GUILD_BANISH_HE_IS_NOT_MY_GUILD_MEMBER(charName));
		return false;
	} else if (player.getObjectId() == legionMember->getObjectId()) {
		utils::PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_GUILD_BANISH_CANT_BANISH_SELF());
		return false;
	} else if (legionMember->isBrigadeGeneral()) {
		utils::PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_GUILD_BANISH_CAN_BANISH_MASTER());
		return false;
	} else if (getRankId(legionMember->getRank()) <= getRankId(player.getLegionMember()->getRank())) {
		utils::PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_GUILD_BANISH_CAN_NOT_BANISH_SAME_MEMBER_RANK());
		return false;
	} else if (!player.getLegionMember()->hasRights(model::team::legion::LegionPermissionsMask::KICK)) {
		utils::PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_GUILD_BANISH_DONT_HAVE_RIGHT_TO_BANISH());
		return false;
	}
	return true;
}

bool LegionService::LegionRestrictions::canAppointBrigadeGeneral(model::gameobjects::player::Player& activePlayer, model::gameobjects::player::Player& targetPlayer) {
	runtime::Ptr<model::team::legion::Legion> legion = activePlayer.getLegion();
	if (!isBrigadeGeneral(activePlayer)) {
		utils::PacketSendUtility::sendPacket(activePlayer, SM_SYSTEM_MESSAGE::STR_GUILD_CHANGE_MASTER_DONT_HAVE_RIGHT());
		return false;
	} else if (activePlayer.equals(targetPlayer)) {
		utils::PacketSendUtility::sendPacket(activePlayer, SM_SYSTEM_MESSAGE::STR_GUILD_CHANGE_MASTER_ERROR_SELF());
		return false;
	} else if (!legion->isMember(targetPlayer.getObjectId())) {
		utils::PacketSendUtility::sendPacket(activePlayer, SM_SYSTEM_MESSAGE::STR_GUILD_CHANGE_MASTER_NOT_MY_GUILD_MEMBER(targetPlayer.getName()));
		return false;
	}
	return true;
}

bool LegionService::LegionRestrictions::canAppointRank(model::gameobjects::player::Player& activePlayer, runtime::Ptr<model::team::legion::LegionMember> targetMember) {
	runtime::Ptr<model::team::legion::Legion> legion = activePlayer.getLegion();
	if (!legion) {
		utils::PacketSendUtility::sendPacket(activePlayer, SM_SYSTEM_MESSAGE::STR_GUILD_CHANGE_MEMBER_RANK_I_AM_NOT_BELONG_TO_GUILD());
		return false;
	} else if (!isBrigadeGeneral(activePlayer)) {
		utils::PacketSendUtility::sendPacket(activePlayer, SM_SYSTEM_MESSAGE::STR_GUILD_CHANGE_MEMBER_RANK_DONT_HAVE_RIGHT());
		return false;
	} else if (!targetMember) {
		utils::PacketSendUtility::sendPacket(activePlayer, SM_SYSTEM_MESSAGE::STR_GUILD_CHANGE_MEMBER_RANK_NO_USER());
		return false;
	} else if (!legion->isMember(targetMember->getObjectId())) {
		utils::PacketSendUtility::sendPacket(activePlayer, SM_SYSTEM_MESSAGE::STR_GUILD_CHANGE_MEMBER_RANK_HE_IS_NOT_MY_GUILD_MEMBER(targetMember->getName()));
		return false;
	} else if (activePlayer.getObjectId() == targetMember->getObjectId()) {
		utils::PacketSendUtility::sendPacket(activePlayer, SM_SYSTEM_MESSAGE::STR_GUILD_CHANGE_MEMBER_RANK_ERROR_SELF());
		return false;
	}
	return true;
}

bool LegionService::LegionRestrictions::canChangeSelfIntro(model::gameobjects::player::Player& /*activePlayer*/, std::string_view newSelfIntro) {
	return isValidSelfIntro(newSelfIntro);
}

bool LegionService::LegionRestrictions::canChangeLevel(model::gameobjects::player::Player& activePlayer) {
	runtime::Ptr<model::team::legion::Legion> legion = activePlayer.getLegion();
	int32_t levelContributionPrice = legion->getContributionPrice();
	if (!activePlayer.getLegionMember()->isBrigadeGeneral()) {
		utils::PacketSendUtility::sendPacket(activePlayer, SM_SYSTEM_MESSAGE::STR_GUILD_CHANGE_LEVEL_DONT_HAVE_RIGHT());
		return false;
	}
	if (legion->getLegionLevel() == MAX_LEGION_LEVEL) {
		utils::PacketSendUtility::sendPacket(activePlayer, SM_SYSTEM_MESSAGE::STR_GUILD_CHANGE_LEVEL_CANT_LEVEL_UP());
		return false;
	}
	if (configs::main::LegionConfig::ENABLE_GUILD_TASK_REQ.load() && legion->getLegionLevel() >= 5) {
		if (!ChallengeTaskService::getInstance().canRaiseLegionLevel(*legion, activePlayer)) {
			utils::PacketSendUtility::sendPacket(activePlayer, SM_SYSTEM_MESSAGE::STR_GUILD_LEVEL_UP_CHALLENGE_TASK(legion->getLegionLevel()));
			return false;
		}
	}
	if (activePlayer.getInventory().getKinah() < legion->getKinahPrice()) {
		utils::PacketSendUtility::sendPacket(activePlayer, SM_SYSTEM_MESSAGE::STR_GUILD_CHANGE_LEVEL_NOT_ENOUGH_MONEY());
		return false;
	}
	if (!legion->hasRequiredMembers()) {
		utils::PacketSendUtility::sendPacket(activePlayer, SM_SYSTEM_MESSAGE::STR_GUILD_CHANGE_LEVEL_NOT_ENOUGH_MEMBER());
		return false;
	}
	if (legion->getContributionPoints() < levelContributionPrice) {
		utils::PacketSendUtility::sendPacket(activePlayer, SM_SYSTEM_MESSAGE::STR_GUILD_CHANGE_LEVEL_NOT_ENOUGH_POINT());
		return false;
	}
	return true;
}

bool LegionService::LegionRestrictions::canChangeNickname(model::gameobjects::player::Player& player, runtime::Ptr<model::team::legion::LegionMember> member, std::string_view memberName, std::string_view newNickname) {
	runtime::Ptr<model::team::legion::Legion> legion = player.getLegion();
	if (!legion) {
		utils::PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_GUILD_CHANGE_MEMBER_NICKNAME_I_AM_NOT_BELONG_TO_GUILD());
		return false;
	} else if (!member || !legion->isMember(member->getObjectId())) {
		utils::PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_GUILD_CHANGE_MEMBER_NICKNAME_HE_IS_NOT_MY_GUILD_MEMBER(memberName));
		return false;
	} else if (!player.getLegionMember()->isBrigadeGeneral()) {
		utils::PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_GUILD_CHANGE_MEMBER_NICKNAME_DONT_HAVE_RIGHT_TO_CHANGE_NICKNAME());
		return false;
	}
	return isValidNickname(newNickname);
}

bool LegionService::LegionRestrictions::canDisbandLegion(model::gameobjects::player::Player& activePlayer) {
	runtime::Ptr<model::team::legion::Legion> legion = activePlayer.getLegion();
	if (!legion) {
		return false;
	}
	if (legion->isDisbanding()) {
		utils::PacketSendUtility::sendPacket(activePlayer, SM_SYSTEM_MESSAGE::STR_GUILD_DISPERSE_ALREADY_REQUESTED());
		return false;
	} else if (!isBrigadeGeneral(activePlayer)) {
		utils::PacketSendUtility::sendPacket(activePlayer, SM_SYSTEM_MESSAGE::STR_GUILD_DISPERSE_ONLY_MASTER_CAN_DISPERSE());
		return false;
	} else if (legion->getLegionWarehouse().getCurrentUser() != 0) {
		utils::PacketSendUtility::sendPacket(activePlayer, SM_SYSTEM_MESSAGE::STR_GUILD_DISPERSE_CANT_DISPERSE_GUILD_WHILE_USING_WAREHOUSE());
		return false;
	} else if (legion->getLegionWarehouse().size() > 0 || legion->getLegionWarehouse().getKinah() > 0) {
		utils::PacketSendUtility::sendPacket(activePlayer, SM_SYSTEM_MESSAGE::STR_GUILD_DISPERSE_CANT_DISPERSE_GUILD_STORE_ITEM_IN_WAREHOUSE());
		return false;
	}
	return true;
}

bool LegionService::LegionRestrictions::canLeave(model::gameobjects::player::Player& activePlayer) {
	if (isBrigadeGeneral(activePlayer)) {
		utils::PacketSendUtility::sendPacket(activePlayer, SM_SYSTEM_MESSAGE::STR_GUILD_LEAVE_MASTER_CANT_LEAVE_BEFORE_CHANGE_MASTER());
		return false;
	} else if (activePlayer.getLegion()->getLegionWarehouse().getCurrentUser() == activePlayer.getObjectId()) {
		utils::PacketSendUtility::sendPacket(activePlayer, SM_SYSTEM_MESSAGE::STR_GUILD_LEAVE_CANT_LEAVE_GUILD_WHILE_USING_WAREHOUSE());
		return false;
	}
	return true;
}

bool LegionService::LegionRestrictions::canRecreateLegion(model::gameobjects::player::Player& activePlayer) {
	if (!isBrigadeGeneral(activePlayer)) {
		utils::PacketSendUtility::sendPacket(activePlayer, SM_SYSTEM_MESSAGE::STR_GUILD_DISPERSE_ONLY_MASTER_CAN_DISPERSE());
		return false;
	} else if (!activePlayer.getLegion()->isDisbanding()) {
		// Legion is not disbanding
		return false;
	}
	return true;
}

bool LegionService::LegionRestrictions::canUploadEmblem(model::gameobjects::player::Player& activePlayer, bool initUpload) {
	if (!canStoreLegionEmblem(activePlayer, MIN_EMBLEM_ID)) {
		return false;
	} else if (activePlayer.getLegion()->getLegionLevel() < 3) {
		// Legion level isn't high enough
		return false;
	} else if (initUpload && activePlayer.getLegion()->getLegionEmblem()->isUploading()) {
		utils::PacketSendUtility::sendPacket(activePlayer, SM_SYSTEM_MESSAGE::STR_GUILD_WARN_FAILURE_UPLOAD_EMBLEM());
		return false;
	} else if (!initUpload && !activePlayer.getLegion()->getLegionEmblem()->isUploading()) {
		utils::PacketSendUtility::sendPacket(activePlayer, SM_SYSTEM_MESSAGE::STR_GUILD_WARN_FAILURE_UPLOAD_EMBLEM());
		return false;
	}
	return true;
}

bool LegionService::LegionRestrictions::canOpenWarehouse(model::gameobjects::player::Player& player, model::gameobjects::Npc& npc) {
	if (!player.isLegionMember()) {
		utils::PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_NO_GUILD_TO_DEPOSIT());
		return false;
	}
	runtime::Ptr<model::team::legion::LegionMember> lm = player.getLegionMember();
	model::team::legion::LegionWarehouse& legWh = lm->getLegion()->getLegionWarehouse();
	if (!configs::main::LegionConfig::LEGION_WAREHOUSE.load() || !npc.getObjectTemplate()->supportsAction(model::DialogAction::OPEN_LEGION_WAREHOUSE)) {
		utils::PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_CANT_USE_GUILD_STORAGE());
		return false;
	} else if (lm->getLegion()->isDisbanding()) {
		utils::PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_GUILD_WAREHOUSE_CANT_USE_WHILE_DISPERSE());
		return false;
	} else if (!lm->hasRights(model::team::legion::LegionPermissionsMask::WH_DEPOSIT) && !lm->hasRights(model::team::legion::LegionPermissionsMask::WH_WITHDRAWAL)) {
		utils::PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_GUILD_WAREHOUSE_NO_RIGHT());
		return false;
	} else if (!legWh.setInUse(player.getObjectId()) && legWh.getCurrentUser() != player.getObjectId()) {
		utils::PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_GUILD_WAREHOUSE_IN_USE());
		return false;
	}
	return true;
}

bool LegionService::LegionRestrictions::canStoreLegionEmblem(model::gameobjects::player::Player& activePlayer, int32_t emblemId) {
	if (emblemId < MIN_EMBLEM_ID || emblemId > MAX_EMBLEM_ID) {
		// Not a valid emblemId
		return false;
	} else if (!isBrigadeGeneral(activePlayer)) {
		utils::PacketSendUtility::sendPacket(activePlayer, SM_SYSTEM_MESSAGE::STR_GUILD_CHANGE_EMBLEM_DONT_HAVE_RIGHT());
		return false;
	} else if (activePlayer.getLegion()->getLegionLevel() < 2) {
		// legion level not high enough
		return false;
	} else if (activePlayer.getInventory().getKinah() <
		trade::PricesService::getPriceForService(configs::main::LegionConfig::LEGION_EMBLEM_REQUIRED_KINAH.load(), activePlayer.getRace())) {
		utils::PacketSendUtility::sendPacket(activePlayer, SM_SYSTEM_MESSAGE::STR_MSG_NOT_ENOUGH_MONEY());
		return false;
	}
	return true;
}

bool LegionService::LegionRestrictions::isBrigadeGeneral(model::gameobjects::player::Player& player) {
	return player.getLegionMember()->isBrigadeGeneral();
}

bool LegionService::LegionRestrictions::isFreeName(std::string_view name) {
	return !dao::LegionDAO::isNameUsed(name);
}

bool LegionService::LegionRestrictions::isValidSelfIntro(std::string_view name) {
	return std::regex_match(commons::configuration::transformers::PatternTransformer::toWide(name), *configs::main::LegionConfig::SELF_INTRO_PATTERN.get());
}

bool LegionService::LegionRestrictions::isValidNickname(std::string_view name) {
	return std::regex_match(commons::configuration::transformers::PatternTransformer::toWide(name), *configs::main::LegionConfig::NICKNAME_PATTERN.get());
}

LegionService::LegionRestrictions::~LegionRestrictions() = default;

void LegionService::storeLegion(model::team::legion::Legion& legion, bool newLegion) {
	if (newLegion) {
		addCachedLegion(legion);
		dao::LegionDAO::saveNewLegion(legion);
	} else {
		dao::LegionDAO::storeLegion(legion);
		runtime::Ptr<model::team::legion::LegionEmblem> emblem = legion.getLegionEmblem();
		if (!emblem) // Java: NullPointerException inside LegionDAO.storeLegionEmblem
			throw runtime::NullPointerException("Legion " + std::to_string(legion.getLegionId()) + " has no emblem");
		dao::LegionDAO::storeLegionEmblem(legion.getLegionId(), *emblem);
	}
}

void LegionService::storeLegion(model::team::legion::Legion& legion) {
	storeLegion(legion, false);
}

void LegionService::storeLegionMember(model::team::legion::LegionMember& legionMember) {
	dao::LegionMemberDAO::storeLegionMember(legionMember);
}

std::vector<runtime::Ptr<model::team::legion::Legion>> LegionService::getCachedLegions() {
	return legionsById.values();
}

void LegionService::addCachedLegion(model::team::legion::Legion& legion) {
	legionsById.put(legion.getLegionId(), runtime::Ref<model::team::legion::Legion>(legion));
}

void LegionService::deleteLegionFromDB(int32_t legionId) {
	dao::LegionDAO::deleteLegion(legionId);
	dao::InventoryDAO::deletePlayerOrLegionItems(legionId);
}

void LegionService::deleteLegionMemberFromDB(model::team::legion::LegionMember& legionMember) {
	legionMemberById.remove(legionMember.getObjectId());
	dao::LegionMemberDAO::deleteLegionMember(legionMember.getObjectId());
	runtime::Ptr<Legion> legion = legionMember.getLegion();
	legion->removeMember(legionMember.getObjectId());
	addHistory(*legion, legionMember.getName(), LegionHistoryAction::KICK);
}

runtime::Ptr<model::team::legion::Legion> LegionService::getLegion(std::string_view legionName) {
	runtime::Ptr<model::team::legion::Legion> legion;
	for (const runtime::Ptr<model::team::legion::Legion>& cached : legionsById.values()) {
		if (commons::utils::StringUtils::equalsIgnoreCase(cached->getName(), legionName)) {
			legion = cached;
			break;
		}
	}
	if (!legion) {
		runtime::Ref<model::team::legion::Legion> loaded = dao::LegionDAO::loadLegion(legionName);
		if (!loaded || checkDisband(*loaded))
			return nullptr;
		loadLegionInfo(*loaded);
		addCachedLegion(*loaded);
		legion = loaded; // retained by the cache
	} else if (checkDisband(*legion)) {
		return nullptr;
	}
	return legion;
}

runtime::Ptr<model::team::legion::Legion> LegionService::getLegion(int32_t legionId) {
	runtime::Ptr<model::team::legion::Legion> legion = legionsById.get(legionId);
	if (!legion) {
		runtime::Ref<model::team::legion::Legion> loaded = dao::LegionDAO::loadLegion(legionId);
		if (!loaded || checkDisband(*loaded))
			return nullptr;
		loadLegionInfo(*loaded);
		addCachedLegion(*loaded);
		legion = loaded; // retained by the cache
	} else if (checkDisband(*legion)) {
		return nullptr;
	}
	return legion;
}

void LegionService::loadLegionInfo(model::team::legion::Legion& legion) {
	legion.setMemberIds(dao::LegionMemberDAO::loadLegionMembers(legion.getLegionId()));
	legion.setAnnouncement(dao::LegionDAO::loadAnnouncement(legion.getLegionId()));
	legion.setLegionEmblem(dao::LegionDAO::loadLegionEmblem(legion.getLegionId()));
	dao::InventoryDAO::loadStorage(legion.getLegionId(), legion.getLegionWarehouse());
	dao::ItemStoneListDAO::load(legion.getLegionWarehouse().getItems());
	dao::LegionDAO::loadHistory(legion);
}

runtime::Ptr<model::team::legion::LegionMember> LegionService::getLegionMember(std::string_view name) {
	runtime::Ref<model::gameobjects::player::PlayerCommonData> playerCommonData = services::player::PlayerService::getOrLoadPlayerCommonData(name);
	return !playerCommonData ? nullptr : getLegionMember(*playerCommonData);
}

runtime::Ptr<model::team::legion::LegionMember> LegionService::getLegionMember(int32_t playerObjId) {
	return getLegionMember(playerObjId, nullptr);
}

runtime::Ptr<model::team::legion::LegionMember> LegionService::getLegionMember(model::gameobjects::player::PlayerCommonData& playerCommonData) {
	return getLegionMember(playerCommonData.getPlayerObjId(), runtime::Ptr<model::gameobjects::player::PlayerCommonData>(playerCommonData));
}

runtime::Ptr<model::team::legion::LegionMember> LegionService::getLegionMember(int32_t playerObjectId, runtime::Ptr<model::gameobjects::player::PlayerCommonData> playerCommonData) {
	// the callback runs under the key's stripe Monitor, like Java's bin lock (ConcurrentHashMap.h conformance site LegionService.java:154-159)
	runtime::Ptr<model::team::legion::LegionMember> legionMember = legionMemberById.computeIfAbsent(playerObjectId, [playerObjectId, &playerCommonData] {
		// lockdep: Java loads the member inside ConcurrentHashMap.computeIfAbsent (LegionService.java:154-159, a ConcurrentHashMap.h conformance site)
		runtime::Ref<model::team::legion::LegionMember> lm = dao::LegionMemberDAO::loadLegionMember(playerObjectId);
		if (lm) {
			if (!playerCommonData) {
				runtime::Ref<model::gameobjects::player::PlayerCommonData> loaded = services::player::PlayerService::getOrLoadPlayerCommonData(playerObjectId);
				if (!loaded) // Java: NullPointerException in LegionMember.setPlayerData
					throw runtime::NullPointerException("No player common data for legion member " + std::to_string(playerObjectId));
				lm->setPlayerData(*loaded);
			} else {
				lm->setPlayerData(*playerCommonData);
			}
		}
		return lm;
	});
	return !legionMember || checkDisband(*legionMember->getLegion()) ? nullptr : legionMember;
}

bool LegionService::checkDisband(model::team::legion::Legion& legion) {
	if (legion.isDisbanding()) {
		if ((commons::utils::currentTimeMillis() / 1000) > legion.getDisbandTime()) {
			disbandLegion(legion);
			return true;
		}
	}
	return false;
}

void LegionService::disbandLegion(model::team::legion::Legion& legion) {
	legionsById.remove(legion.getLegionId());
	for (int32_t memberId : *legion.getMemberIds())
		legionMemberById.remove(memberId);
	SiegeService::getInstance().cleanLegionId(legion.getLegionId());
	deleteLegionFromDB(legion.getLegionId());
	updateAfterDisbandLegion(legion);
}

// anonymous RequestResponseHandler at LegionService.java:191 (fieldmap key LegionService$1); local disbandResponseHandler; storage: stored in ResponseRequester
void LegionService::requestDisbandLegion(model::gameobjects::Npc& npc, model::gameobjects::player::Player& activePlayer) {
	if (legionRestrictions->canDisbandLegion(activePlayer)) {
		runtime::Ref<LegionService_DisbandResponseHandler> disbandResponseHandler = LegionService_DisbandResponseHandler::create(npc);

		bool disbandResult = activePlayer.getResponseRequester().putRequest(SM_QUESTION_WINDOW::STR_GUILD_DISPERSE_STAYMODE, disbandResponseHandler);
		if (disbandResult) {
			utils::PacketSendUtility::sendPacket(activePlayer, SM_QUESTION_WINDOW(SM_QUESTION_WINDOW::STR_GUILD_DISPERSE_STAYMODE, 0, 0));
		}
	}
}

void LegionService::createLegion(model::gameobjects::player::Player& activePlayer, std::string_view legionName) {
	if (legionRestrictions->canCreateLegion(activePlayer, legionName)) {
		runtime::Ref<Legion> legion = Legion::create(utils::idfactory::IDFactory::getInstance().nextId(), legionName);
		legion->addLegionMember(activePlayer.getObjectId());

		activePlayer.getInventory().decreaseKinah(configs::main::LegionConfig::LEGION_CREATE_REQUIRED_KINAH.load());

		storeLegion(*legion, true);
		addLegionMember(*legion, activePlayer, LegionRank::BRIGADE_GENERAL);
		addHistory(*legion, "", LegionHistoryAction::CREATE);
		addHistory(*legion, activePlayer.getName(), LegionHistoryAction::JOIN);

		utils::PacketSendUtility::sendPacket(activePlayer, SM_SYSTEM_MESSAGE::STR_GUILD_CREATED(legion->getName()));
	}
}

bool LegionService::addToLegion(model::team::legion::Legion& legion, model::gameobjects::player::Player& invited, model::gameobjects::player::Player& inviter) {
	int32_t playerObjId = invited.getObjectId();
	if (legion.addLegionMember(playerObjId)) {
		// Bind LegionMember to Player
		addLegionMember(legion, invited);

		// Display current announcement
		displayLegionAnnouncement(invited, legion.getAnnouncement());

		// Add to history of legion
		addHistory(legion, invited.getName(), LegionHistoryAction::JOIN);
		return true;
	}
	utils::PacketSendUtility::sendPacket(inviter, SM_SYSTEM_MESSAGE::STR_GUILD_INVITE_CAN_NOT_ADD_MEMBER_ANY_MORE());
	return false;
}

// anonymous RequestResponseHandler at LegionService.java:246 (fieldmap key LegionService$2); local responseHandler; storage: stored in ResponseRequester
void LegionService::invitePlayerToLegion(model::gameobjects::player::Player& activePlayer, std::string_view targetName) {
	runtime::Ptr<Player> targetPlayer = world::World::getInstance().getPlayer(targetName);
	if (legionRestrictions->canInvitePlayer(activePlayer, targetPlayer)) {
		runtime::Ptr<Legion> legion = activePlayer.getLegion();
		runtime::Ref<LegionService_InviteResponseHandler> responseHandler = LegionService_InviteResponseHandler::create(activePlayer, *legion);

		bool requested = targetPlayer->getResponseRequester().putRequest(SM_QUESTION_WINDOW::STR_GUILD_INVITE_DO_YOU_ACCEPT_INVITATION, responseHandler);
		// If the player is busy and could not be asked
		if (!requested) {
			utils::PacketSendUtility::sendPacket(activePlayer, SM_SYSTEM_MESSAGE::STR_GUILD_INVITE_OTHER_IS_BUSY());
		} else {
			utils::PacketSendUtility::sendPacket(activePlayer, SM_SYSTEM_MESSAGE::STR_GUILD_INVITE_SENT_INVITE_MSG_TO_HIM(targetPlayer->getName()));

			// Send question packet to buddy
			utils::PacketSendUtility::sendPacket(*targetPlayer, SM_QUESTION_WINDOW(SM_QUESTION_WINDOW::STR_GUILD_INVITE_DO_YOU_ACCEPT_INVITATION, 0, 0,
				legion->getName(), std::to_string(legion->getLegionLevel()), activePlayer.getName()));
		}
	}
}

void LegionService::displayLegionAnnouncement(model::gameobjects::player::Player& targetPlayer, runtime::Ptr<model::team::legion::Legion::Announcement> announcement) {
	if (announcement)
		utils::PacketSendUtility::sendPacket(targetPlayer,
			SM_SYSTEM_MESSAGE::STR_GUILD_NOTICE(announcement->message(), announcement->time().time_since_epoch().count() / 1000));
}

// anonymous RequestResponseHandler at LegionService.java:288 (fieldmap key LegionService$3); local responseHandler; storage: stored in ResponseRequester
void LegionService::startBrigadeGeneralChangeProcess(model::gameobjects::player::Player& legionLeader, std::string_view memberName) {
	runtime::Ptr<Player> newLegionLeader = world::World::getInstance().getPlayer(memberName);
	if (!newLegionLeader) {
		utils::PacketSendUtility::sendPacket(legionLeader, SM_SYSTEM_MESSAGE::STR_GUILD_CHANGE_MASTER_NO_SUCH_USER());
		return;
	}
	runtime::Ref<LegionService_ChangeProcessResponseHandler> responseHandler = LegionService_ChangeProcessResponseHandler::create(*newLegionLeader);
	bool requested = legionLeader.getResponseRequester().putRequest(904979, responseHandler);
	if (requested) {
		utils::PacketSendUtility::sendPacket(legionLeader, SM_QUESTION_WINDOW(904979, 0, 0, newLegionLeader->getName()));
	}
}

// anonymous RequestResponseHandler at LegionService.java:303 (fieldmap key LegionService$4); local responseHandler; storage: stored in ResponseRequester
void LegionService::appointBrigadeGeneral(model::gameobjects::player::Player& activePlayer, model::gameobjects::player::Player& targetPlayer) {
	if (legionRestrictions->canAppointBrigadeGeneral(activePlayer, targetPlayer)) {
		runtime::Ref<LegionService_AppointResponseHandler> responseHandler = LegionService_AppointResponseHandler::create(activePlayer);

		bool requested = targetPlayer.getResponseRequester().putRequest(SM_QUESTION_WINDOW::STR_GUILD_CHANGE_MASTER_DO_YOU_ACCEPT_OFFER, responseHandler);
		// If the player is busy and could not be asked
		if (!requested) {
			utils::PacketSendUtility::sendPacket(activePlayer, SM_SYSTEM_MESSAGE::STR_GUILD_CHANGE_MASTER_SENT_CANT_OFFER_WHEN_HE_IS_QUESTION_ASKED());
		} else {
			utils::PacketSendUtility::sendPacket(activePlayer, SM_SYSTEM_MESSAGE::STR_GUILD_CHANGE_MASTER_SENT_OFFER_MSG_TO_HIM(targetPlayer.getName()));

			// Send question packet to buddy
			// TODO: Add char name parameter? Doesn't work?
			utils::PacketSendUtility::sendPacket(targetPlayer, SM_QUESTION_WINDOW(SM_QUESTION_WINDOW::STR_GUILD_CHANGE_MASTER_DO_YOU_ACCEPT_OFFER,
				activePlayer.getObjectId(), 0, activePlayer.getName()));
		}
	}
}

void LegionService::appointBrigadeGeneral(model::team::legion::LegionMember& member) {
	if (member.isBrigadeGeneral())
		return;
	runtime::Ptr<Legion> legion = member.getLegion();
	runtime::Ptr<LegionMember> prevBrigadeGeneral = legion->getBrigadeGeneral();
	prevBrigadeGeneral->setRank(LegionRank::CENTURION);
	if (!prevBrigadeGeneral->isOnline())
		dao::LegionMemberDAO::storeLegionMember(*prevBrigadeGeneral);
	utils::PacketSendUtility::broadcastToLegion(*legion, SM_LEGION_UPDATE_MEMBER(*prevBrigadeGeneral));
	member.setRank(LegionRank::BRIGADE_GENERAL);
	utils::PacketSendUtility::broadcastToLegion(*legion, SM_LEGION_UPDATE_MEMBER(member, 1300273, member.getName()));
	utils::PacketSendUtility::broadcastToLegion(*legion, SM_LEGION_EDIT(0x08));
	addHistory(*legion, member.getName(), LegionHistoryAction::APPOINTED);
}

void LegionService::appointRank(model::gameobjects::player::Player& player, std::string_view charName, int32_t rankId) {
	runtime::Ptr<LegionMember> legionMember = getLegionMember(charName);
	if (legionRestrictions->canAppointRank(player, legionMember)) {
		// Java: LegionRank.values()[rankId]
		if (rankId < 0 || rankId >= static_cast<int32_t>(xml::EnumTraits<LegionRank>::names.size()))
			throw runtime::ArrayIndexOutOfBoundsException("Index " + std::to_string(rankId) + " out of bounds for length " +
				std::to_string(xml::EnumTraits<LegionRank>::names.size()));
		LegionRank rank = static_cast<LegionRank>(rankId);
		int32_t msgId;
		switch (rank) {
			case LegionRank::DEPUTY:
				msgId = 1400902;
				break;
			case LegionRank::LEGIONARY:
				msgId = 1300268;
				break;
			case LegionRank::CENTURION:
				msgId = 1300267;
				break;
			case LegionRank::VOLUNTEER:
				msgId = 1400903;
				break;
			default:
				msgId = 0;
				break;
		}
		legionMember->setRank(rank);
		if (!legionMember->isOnline())
			dao::LegionMemberDAO::storeLegionMember(*legionMember);
		utils::PacketSendUtility::broadcastToLegion(*legionMember->getLegion(), SM_LEGION_UPDATE_MEMBER(*legionMember, msgId, legionMember->getName()));
	}
}

void LegionService::changeSelfIntro(model::gameobjects::player::Player& activePlayer, std::string_view newSelfIntro) {
	if (legionRestrictions->canChangeSelfIntro(activePlayer, newSelfIntro)) {
		runtime::Ptr<LegionMember> legionMember = activePlayer.getLegionMember();
		legionMember->setSelfIntro(newSelfIntro);
		utils::PacketSendUtility::broadcastToLegion(*legionMember->getLegion(), SM_LEGION_UPDATE_SELF_INTRO(activePlayer.getObjectId(), newSelfIntro));
		utils::PacketSendUtility::sendPacket(activePlayer, SM_SYSTEM_MESSAGE::STR_GUILD_WRITE_INTRO_DONE());
	}
}

void LegionService::changePermissions(model::gameobjects::player::Player& player, int16_t deputyPermission, int16_t centurionPermission,
	int16_t legionarPermission, int16_t volunteerPermission) {
	runtime::Ptr<LegionMember> legionMember = player.getLegionMember();
	if (!legionMember || !legionMember->isBrigadeGeneral()) {
		utils::PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_GUILD_CHANGE_RIGHT_DONT_HAVE_RIGHT());
		return;
	}
	legionMember->getLegion()->setLegionPermissions(deputyPermission, centurionPermission, legionarPermission, volunteerPermission);
	utils::PacketSendUtility::broadcastToLegion(*legionMember->getLegion(), SM_LEGION_EDIT(0x02, *legionMember->getLegion()));
}

void LegionService::requestChangeLevel(model::gameobjects::player::Player& activePlayer) {
	if (legionRestrictions->canChangeLevel(activePlayer)) {
		runtime::Ptr<Legion> legion = activePlayer.getLegion();
		activePlayer.getInventory().decreaseKinah(legion->getKinahPrice());
		changeLevel(*legion, legion->getLegionLevel() + 1, false);
		addHistory(*legion, std::to_string(legion->getLegionLevel()), LegionHistoryAction::LEVEL_UP);
	}
}

void LegionService::changeLevel(model::team::legion::Legion& legion, int32_t newLevel, bool save) {
	legion.setLegionLevel(newLevel);
	utils::PacketSendUtility::broadcastToLegion(legion, SM_LEGION_EDIT(0x00, legion));
	utils::PacketSendUtility::broadcastToLegion(legion, SM_SYSTEM_MESSAGE::STR_GUILD_EVENT_LEVELUP(newLevel));
	if (save)
		storeLegion(legion);
}

void LegionService::changeNickname(model::gameobjects::player::Player& activePlayer, std::string_view memberName, std::string_view newNickname) {
	runtime::Ptr<LegionMember> legionMember = getLegionMember(memberName);
	if (legionRestrictions->canChangeNickname(activePlayer, legionMember, memberName, newNickname)) {
		legionMember->setNickname(newNickname);
		utils::PacketSendUtility::broadcastToLegion(*legionMember->getLegion(), SM_LEGION_UPDATE_NICKNAME(legionMember->getObjectId(), newNickname));
		if (!legionMember->isOnline())
			dao::LegionMemberDAO::storeLegionMember(*legionMember);
	}
}

void LegionService::updateAfterDisbandLegion(model::team::legion::Legion& legion) {
	for (const runtime::Ptr<Player>& onlineLegionMember : legion.getOnlinePlayers()) {
		utils::PacketSendUtility::broadcastPacket(*onlineLegionMember,
			SM_LEGION_UPDATE_TITLE(onlineLegionMember->getObjectId(), 0, "", onlineLegionMember->getLegionMember()->getRank()), true);
		utils::PacketSendUtility::sendPacket(*onlineLegionMember, SM_LEGION_LEAVE_MEMBER(1300302, 0, legion.getName()));
		onlineLegionMember->resetLegionMember();
		conquerorAndProtectorSystem::ConquerorAndProtectorService::getInstance().onLeaveLegion(*onlineLegionMember);
	}
}

void LegionService::updateMembersEmblem(model::team::legion::Legion& legion) {
	runtime::Ptr<LegionEmblem> legionEmblem = legion.getLegionEmblem();
	for (const runtime::Ptr<Player>& onlineLegionMember : legion.getOnlinePlayers()) {
		utils::PacketSendUtility::broadcastPacket(*onlineLegionMember, SM_LEGION_UPDATE_EMBLEM(legion.getLegionId(), *legionEmblem), true);
		if (legionEmblem->getEmblemType() == LegionEmblemType::CUSTOM)
			sendEmblemData(*onlineLegionMember, *legionEmblem, legion.getLegionId(), legion.getName());
	}
}

void LegionService::updateMembersOfDisbandLegion(model::team::legion::Legion& legion, int32_t unixTime) {
	for (const runtime::Ptr<Player>& onlineLegionMember : legion.getOnlinePlayers()) {
		utils::PacketSendUtility::sendPacket(*onlineLegionMember, SM_LEGION_UPDATE_MEMBER(*onlineLegionMember, 1300303, std::to_string(unixTime)));
		utils::PacketSendUtility::sendPacket(*onlineLegionMember, SM_LEGION_EDIT(0x06, unixTime));
	}
}

void LegionService::updateMembersOfRecreateLegion(model::team::legion::Legion& legion) {
	for (const runtime::Ptr<Player>& onlineLegionMember : legion.getOnlinePlayers()) {
		utils::PacketSendUtility::sendPacket(*onlineLegionMember, SM_LEGION_UPDATE_MEMBER(*onlineLegionMember, 1300307, ""));
		utils::PacketSendUtility::sendPacket(*onlineLegionMember, SM_LEGION_EDIT(0x07));
	}
}

void LegionService::storeLegionEmblem(model::gameobjects::player::Player& activePlayer, int32_t emblemId, int32_t color_a, int32_t color_r,
	int32_t color_g, int32_t color_b, model::team::legion::LegionEmblemType emblemType) {
	if (legionRestrictions->canStoreLegionEmblem(activePlayer, emblemId)) {
		runtime::Ptr<Legion> legion = activePlayer.getLegion();
		addHistory(*legion, "", LegionHistoryAction::EMBLEM_MODIFIED);
		activePlayer.getInventory().decreaseKinah(
			trade::PricesService::getPriceForService(configs::main::LegionConfig::LEGION_EMBLEM_REQUIRED_KINAH.load(), activePlayer.getRace()));
		legion->getLegionEmblem()->setEmblem(emblemId, color_a, color_r, color_g, color_b, emblemType, nullptr);
		updateMembersEmblem(*legion);
		utils::PacketSendUtility::sendPacket(activePlayer, SM_SYSTEM_MESSAGE::STR_GUILD_CHANGE_EMBLEM());
	}
}

void LegionService::openLegionWarehouse(model::gameobjects::player::Player& player, model::gameobjects::Npc& npc) {
	if (legionRestrictions->canOpenWarehouse(player, npc)) {
		LegionWhUpdate(player);
		utils::PacketSendUtility::sendPacket(player, SM_LEGION_EDIT(0x04, *player.getLegion())); // kinah
		int32_t whLvl = player.getLegion()->getWarehouseExpansions();
		std::vector<runtime::Ref<model::gameobjects::Item>> items;
		for (const runtime::Ptr<model::gameobjects::Item>& item : player.getLegion()->getLegionWarehouse().getItems())
			items.emplace_back(item);
		const bool itemsEmpty = items.empty();
		int32_t storageId = model::items::storage::getId(model::items::storage::StorageType::LEGION_WAREHOUSE);

		utils::collections::FixedElementCountSplitList<model::gameobjects::Item> legionMemberSplitList(std::move(items), false, 10);
		for (utils::collections::ListPart<model::gameobjects::Item>& part : legionMemberSplitList)
			utils::PacketSendUtility::sendPacket(player, SM_WAREHOUSE_INFO(part.borrowed(), storageId, whLvl, part.isFirst(), player));
		utils::PacketSendUtility::sendPacket(player, SM_WAREHOUSE_INFO({}, storageId, whLvl, itemsEmpty, player)); // Java: null items
		utils::PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(npc.getObjectId(), id(model::DialogPage::LEGION_WAREHOUSE)));
	}
}

// anonymous RequestResponseHandler at LegionService.java:498 (fieldmap key LegionService$5); local disbandResponseHandler; storage: stored in ResponseRequester
void LegionService::recreateLegion(model::gameobjects::Npc& npc, model::gameobjects::player::Player& activePlayer) {
	if (legionRestrictions->canRecreateLegion(activePlayer)) {
		runtime::Ref<LegionService_RecreateResponseHandler> disbandResponseHandler = LegionService_RecreateResponseHandler::create(npc);

		bool disbandResult = activePlayer.getResponseRequester().putRequest(SM_QUESTION_WINDOW::STR_GUILD_DISPERSE_STAYMODE_CANCEL, disbandResponseHandler);
		if (disbandResult) {
			utils::PacketSendUtility::sendPacket(activePlayer, SM_QUESTION_WINDOW(SM_QUESTION_WINDOW::STR_GUILD_DISPERSE_STAYMODE_CANCEL, 0, 0));
		}
	}
}

void LegionService::LegionWhUpdate(model::gameobjects::player::Player& player) {
	runtime::Ptr<model::team::legion::Legion> legion = player.getLegion();

	if (!legion)
		return;

	std::vector<runtime::Ptr<model::gameobjects::Item>> allItems = legion->getLegionWarehouse().getItemsWithKinah();
	for (const runtime::Ptr<model::gameobjects::Item>& item : legion->getLegionWarehouse().getDeletedItems().snapshot())
		allItems.push_back(item);
	try {
		dao::InventoryDAO::store(allItems, player.getObjectId(), player.getAccount()->getId(), legion->getLegionId());
		dao::ItemStoneListDAO::save(allItems);
	} catch (const std::exception& ex) {
		log.error("Exception during periodic saving of legion WH", ex);
	}
}

void LegionService::updateMemberInfo(model::gameobjects::player::Player& player) {
	runtime::Ptr<LegionMember> legionMember = player.getLegionMember();
	legionMember->setPlayerData(player);
	utils::PacketSendUtility::broadcastToLegion(*player.getLegion(), SM_LEGION_UPDATE_MEMBER(*legionMember));
}

void LegionService::setContributionPoints(model::team::legion::Legion& legion, int64_t newPoints, bool save) {
	legion.setContributionPoints(newPoints);
	utils::PacketSendUtility::broadcastToLegion(legion, SM_LEGION_EDIT(0x03, legion));
	if (save)
		storeLegion(legion);
}

void LegionService::uploadEmblemInfo(model::gameobjects::player::Player& activePlayer, int32_t totalSize, int32_t color_a, int32_t color_r,
	int32_t color_g, int32_t color_b, model::team::legion::LegionEmblemType emblemType) {
	runtime::Ptr<LegionEmblem> legionEmblem = activePlayer.getLegion()->getLegionEmblem();
	if (legionRestrictions->canUploadEmblem(activePlayer, true)) {
		legionEmblem->resetUploadSettings();
		legionEmblem->setEmblem(legionEmblem->getEmblemId(), color_a, color_r, color_g, color_b, emblemType, nullptr);
		legionEmblem->setUploadSize(totalSize);
		legionEmblem->setUploading(true);
	} else {
		legionEmblem->resetUploadSettings();
	}
}

void LegionService::uploadEmblemData(model::gameobjects::player::Player& activePlayer, int32_t size, std::span<const uint8_t> data) {
	runtime::Ptr<LegionEmblem> legionEmblem = activePlayer.getLegion()->getLegionEmblem();
	if (legionRestrictions->canUploadEmblem(activePlayer, false)) {
		legionEmblem->addUploadedSize(size);
		// Java byte[]: the received bytes as a runtime array
		runtime::Ref<runtime::Array<int8_t>> chunk = runtime::Array<int8_t>::make(static_cast<int32_t>(data.size()));
		for (int32_t i = 0; i < static_cast<int32_t>(data.size()); i++)
			(*chunk)[i] = static_cast<int8_t>(data[static_cast<size_t>(i)]);
		legionEmblem->addUploadData(chunk);

		if (legionEmblem->getUploadedSize() >= legionEmblem->getUploadSize()) {
			if (legionEmblem->getUploadedSize() == 0 || legionEmblem->getUploadedSize() > legionEmblem->getUploadSize()) {
				utils::PacketSendUtility::sendPacket(activePlayer, SM_SYSTEM_MESSAGE::STR_GUILD_WARN_CORRUPT_EMBLEM_FILE());
				return;
			}
			activePlayer.getInventory().decreaseKinah(
				trade::PricesService::getPriceForService(configs::main::LegionConfig::LEGION_EMBLEM_REQUIRED_KINAH.load(), activePlayer.getRace()));
			// Finished
			legionEmblem->setCustomEmblemData(legionEmblem->getUploadData());
			dao::LegionDAO::storeLegionEmblem(activePlayer.getLegion()->getLegionId(), *legionEmblem);
			addHistory(*activePlayer.getLegion(), "", LegionHistoryAction::EMBLEM_REGISTER);
			updateMembersEmblem(*activePlayer.getLegion());
			utils::PacketSendUtility::sendPacket(activePlayer, SM_SYSTEM_MESSAGE::STR_GUILD_WARN_SUCCESS_UPLOAD_EMBLEM());
			legionEmblem->resetUploadSettings();
		}
	} else {
		utils::PacketSendUtility::sendPacket(activePlayer, SM_SYSTEM_MESSAGE::STR_GUILD_WARN_FAILURE_UPLOAD_EMBLEM());
		legionEmblem->resetUploadSettings();
	}
}

void LegionService::sendEmblemData(model::gameobjects::player::Player& player, model::team::legion::LegionEmblem& legionEmblem, int32_t legionId,
	std::string_view legionName) {
	runtime::Ptr<runtime::Array<int8_t>> customEmblemData = legionEmblem.getCustomEmblemData();
	int32_t dataLength = !customEmblemData ? 0 : customEmblemData->length();
	utils::PacketSendUtility::sendPacket(player, SM_LEGION_SEND_EMBLEM(legionId, legionEmblem, dataLength, legionName));
	if (dataLength > 0) {
		// Java: a ByteBuffer over a copy of the data, read in chunks of at most 7993 bytes
		std::vector<uint8_t> buf;
		buf.reserve(static_cast<size_t>(dataLength));
		for (int8_t value : *customEmblemData)
			buf.push_back(static_cast<uint8_t>(value));
		int32_t position = 0;
		log.debug("legionEmblem size: " + std::to_string(buf.size()) + " bytes");
		int32_t maxSize = 7993;
		int32_t currentSize;
		do {
			log.debug("legionEmblem data position: " + std::to_string(position));
			currentSize = dataLength - position;
			log.debug("legionEmblem data remaining capacity: " + std::to_string(currentSize) + " bytes");

			if (currentSize >= maxSize) {
				std::span<const uint8_t> bytes(buf.data() + position, static_cast<size_t>(maxSize));
				position += maxSize;
				log.debug("legionEmblem data send size: " + std::to_string(bytes.size()) + " bytes");
				utils::PacketSendUtility::sendPacket(player, SM_LEGION_SEND_EMBLEM_DATA(maxSize, bytes));
			} else {
				std::span<const uint8_t> bytes(buf.data() + position, static_cast<size_t>(currentSize));
				position += currentSize;
				log.debug("legionEmblem data send size: " + std::to_string(bytes.size()) + " bytes");
				utils::PacketSendUtility::sendPacket(player, SM_LEGION_SEND_EMBLEM_DATA(currentSize, bytes));
			}
		} while (dataLength != position);
	}
}

void LegionService::changeAnnouncement(model::gameobjects::player::Player& activePlayer, std::string_view messageValue) {
	if (!activePlayer.getLegionMember()->hasRights(LegionPermissionsMask::EDIT)) {
		utils::PacketSendUtility::sendPacket(activePlayer, SM_SYSTEM_MESSAGE::STR_GUILD_WRITE_NOTICE_DONT_HAVE_RIGHT());
		return;
	}
	runtime::Ptr<Legion> legion = activePlayer.getLegion();
	runtime::Ref<Legion::Announcement> announcement;
	std::string message(messageValue);
	if (!message.empty()) {
		// Java: String.length() and substring(0, 256) count UTF-16 code units
		const int32_t length = commons::utils::StringUtils::utf16Length(message);
		if (length > 256) {
			log.warn("Truncated legion announcement sent by " + activePlayer.toString() + " (old length: " + std::to_string(length) + ")");
			message = commons::utils::StringUtils::substring(message, 0, 256);
		}
		announcement = Legion::Announcement::create(message, commons::database::Timestamp(std::chrono::milliseconds(commons::utils::currentTimeMillis())));
	}
	legion->setAnnouncement(announcement);
	dao::LegionDAO::saveAnnouncement(legion->getLegionId(), announcement);
	if (!announcement) {
		utils::PacketSendUtility::sendPacket(activePlayer, SM_SYSTEM_MESSAGE::STR_MSG_CLEAR_GUILD_NOTICE());
		utils::PacketSendUtility::broadcastToLegion(*legion, SM_LEGION_INFO(*legion), activePlayer.getObjectId());
	} else {
		utils::PacketSendUtility::sendPacket(activePlayer, SM_SYSTEM_MESSAGE::STR_GUILD_WRITE_NOTICE_DONE());
		utils::PacketSendUtility::broadcastToLegion(*legion, SM_LEGION_EDIT(*announcement));
	}
}

void LegionService::addHistory(model::team::legion::Legion& legion, std::string_view text, model::team::legion::LegionHistoryAction action) {
	addHistory(legion, text, action, "");
}

void LegionService::addRewardHistory(model::team::legion::Legion& legion, int64_t kinahAmount, model::team::legion::LegionHistoryAction action,
	int32_t fortressId) {
	addHistory(legion, std::to_string(kinahAmount), action, std::to_string(fortressId));
}

void LegionService::addHistory(model::team::legion::Legion& legion, std::string_view name, model::team::legion::LegionHistoryAction action,
	std::string_view description) {
	runtime::Ref<model::team::legion::LegionHistoryEntry> historyEntry = dao::LegionDAO::insertHistory(legion.getLegionId(), action, name, description);
	std::vector<runtime::Ref<model::team::legion::LegionHistoryEntry>> removedEntries = legion.addHistory(*historyEntry);
	dao::LegionDAO::deleteHistory(legion.getLegionId(), std::vector<runtime::Ptr<model::team::legion::LegionHistoryEntry>>(removedEntries.begin(), removedEntries.end()));
	utils::PacketSendUtility::broadcastToLegion(legion, SM_LEGION_HISTORY(legion.getHistory(getType(action)), getType(action)));
}

void LegionService::addLegionMember(model::team::legion::Legion& legion, model::gameobjects::player::Player& player) {
	addLegionMember(legion, player, LegionRank::VOLUNTEER);
}

void LegionService::addLegionMember(model::team::legion::Legion& legion, model::gameobjects::player::Player& player, model::team::legion::LegionRank rank) {
	// Set legion member of player and save in the database
	player.setLegionMember(LegionMember::create(player.getObjectId(), legion));
	player.getLegionMember()->setPlayerData(player);
	player.getLegionMember()->setRank(rank);
	dao::LegionMemberDAO::saveNewLegionMember(*player.getLegionMember());
	legionMemberById.put(player.getObjectId(), runtime::Ref<LegionMember>(player.getLegionMember()));

	// Send the new legion member the required legion packets
	utils::PacketSendUtility::sendPacket(player, SM_LEGION_INFO(legion));
	// do not include invited player in member list since he will be added via SM_LEGION_ADD_MEMBER
	updateLegionMemberList(runtime::Ptr<Player>(player), false, player.getObjectId());

	// Send legion member info to the members
	utils::PacketSendUtility::broadcastToLegion(legion, SM_LEGION_ADD_MEMBER(player, false, 1300260, player.getName()));
	// Send legion emblem information
	runtime::Ptr<LegionEmblem> legionEmblem = legion.getLegionEmblem();
	utils::PacketSendUtility::broadcastPacket(player, SM_LEGION_UPDATE_EMBLEM(legion.getLegionId(), *legionEmblem), true);

	// Send legion edit
	utils::PacketSendUtility::broadcastToLegion(legion, SM_LEGION_EDIT(0x08));

	// Update legion member's appearance in game
	utils::PacketSendUtility::broadcastPacket(player,
		SM_LEGION_UPDATE_TITLE(player.getObjectId(), legion.getLegionId(), legion.getName(), player.getLegionMember()->getRank()), true);
	legion.addBonus();
}

bool LegionService::removeLegionMember(model::gameobjects::player::Player& player) {
	return removeLegionMember(player.getLegionMember(), std::nullopt);
}

bool LegionService::removeLegionMember(runtime::Ptr<model::team::legion::LegionMember> legionMember, std::optional<std::string_view> kickerName) {
	if (!legionMember)
		return false;
	// Delete legion member from database and cache
	deleteLegionMemberFromDB(*legionMember);

	runtime::Ptr<Legion> legion = legionMember->getLegion();
	legion->getLegionWarehouse().unsetInUse(legionMember->getObjectId());

	if (kickerName) {
		utils::PacketSendUtility::broadcastToLegion(*legion,
			SM_LEGION_LEAVE_MEMBER(1300247, legionMember->getObjectId(), *kickerName, legionMember->getName()), legionMember->getObjectId());
	} else {
		utils::PacketSendUtility::broadcastToLegion(*legion,
			SM_LEGION_LEAVE_MEMBER(1300240, legionMember->getObjectId(), legionMember->getName(), legion->getName()), legionMember->getObjectId());
	}
	runtime::Ptr<Player> player = world::World::getInstance().getPlayer(legionMember->getObjectId());
	if (player) {
		utils::PacketSendUtility::sendPacket(*player, SM_LEGION_LEAVE_MEMBER(kickerName ? 1300246 : 1300241, 0, legion->getName()));
		utils::PacketSendUtility::broadcastPacket(*player, SM_LEGION_UPDATE_TITLE(player->getObjectId(), 0, "", legionMember->getRank()), true);
		if (legion->hasBonus())
			utils::PacketSendUtility::sendPacket(*player, network::aion::serverpackets::SM_ICON_INFO(1, false));
		player->resetLegionMember();
		conquerorAndProtectorSystem::ConquerorAndProtectorService::getInstance().onLeaveLegion(*player);
	}
	legion->removeBonus();
	return true;
}

void LegionService::kickMember(model::gameobjects::player::Player& player, std::string_view memberName) {
	runtime::Ptr<LegionMember> legionMember = getLegionMember(memberName);
	if (legionRestrictions->canKickPlayer(player, memberName, legionMember))
		removeLegionMember(legionMember, player.getName());
}

bool LegionService::leaveLegion(model::gameobjects::player::Player& player, bool skipChecks) {
	if (skipChecks || legionRestrictions->canLeave(player))
		return removeLegionMember(player);
	return false;
}

void LegionService::onLogin(model::gameobjects::player::Player& activePlayer) {
	runtime::Ptr<Legion> legion = activePlayer.getLegion();

	// Tell all legion members player has come online
	LegionService::getInstance().updateMemberInfo(activePlayer);

	// Notify legion members player has logged in
	utils::PacketSendUtility::broadcastToLegion(*legion, SM_SYSTEM_MESSAGE::STR_MSG_NOTIFY_LOGIN_GUILD(activePlayer.getName()), activePlayer.getObjectId());

	// Send member add to player
	utils::PacketSendUtility::broadcastToLegion(*legion, SM_LEGION_ADD_MEMBER(activePlayer, true, 0, ""));

	// Send legion info packets
	utils::PacketSendUtility::sendPacket(activePlayer, SM_LEGION_INFO(*legion));
	updateLegionMemberList(activePlayer, false);

	// Send current announcement to player
	displayLegionAnnouncement(activePlayer, legion->getAnnouncement());

	if (legion->isDisbanding())
		utils::PacketSendUtility::sendPacket(activePlayer, SM_LEGION_EDIT(0x06, legion->getDisbandTime()));

	if (legion->hasBonus()) {
		utils::PacketSendUtility::sendPacket(activePlayer, network::aion::serverpackets::SM_ICON_INFO(1, true));
	} else {
		legion->addBonus();
	}
}

void LegionService::onLogout(model::gameobjects::player::Player& player) {
	runtime::Ptr<LegionMember> legionMember = player.getLegionMember();
	runtime::Ptr<Legion> legion = legionMember->getLegion();
	legion->getLegionWarehouse().unsetInUse(player.getObjectId());
	updateMemberInfo(player);
	storeLegion(*legion);
	storeLegionMember(*player.getLegionMember());
	legion->removeBonus();
}

void LegionService::addWHItemHistory(model::gameobjects::player::Player& player, int32_t itemId, int64_t value, model::items::storage::IStorage& sourceStorage, model::items::storage::IStorage& destStorage) {
	runtime::Ptr<model::team::legion::Legion> legion = player.getLegion();
	if (legion) {
		std::string description = std::to_string(itemId) + ":" + std::to_string(value); // Java: itemId + ":" + count
		if (sourceStorage.getStorageType() == model::items::storage::StorageType::LEGION_WAREHOUSE) {
			addHistory(*legion, player.getName(), model::team::legion::LegionHistoryAction::ITEM_WITHDRAW, description);
		} else if (destStorage.getStorageType() == model::items::storage::StorageType::LEGION_WAREHOUSE) {
			addHistory(*legion, player.getName(), model::team::legion::LegionHistoryAction::ITEM_DEPOSIT, description);
		}
	}
}

void LegionService::updateLegionMemberList(model::gameobjects::player::Player& player, bool broadcastToLegion) {
	updateLegionMemberList(runtime::Ptr<Player>(player), broadcastToLegion, std::nullopt);
}

void LegionService::updateLegionMemberList(runtime::Ptr<model::gameobjects::player::Player> player, bool broadcastToLegion,
	std::optional<int32_t> excludedPlayerId) {
	if (player && player->getLegion()) {
		runtime::Ptr<Legion> legion = player->getLegion();
		std::vector<runtime::Ptr<LegionMember>> allMembers = legion->getMembers();
		if (excludedPlayerId)
			std::erase_if(allMembers, [&](const runtime::Ptr<LegionMember>& member) { return member->getObjectId() == *excludedPlayerId; });
		std::vector<runtime::Ref<LegionMember>> members(allMembers.begin(), allMembers.end());
		utils::collections::FixedElementCountSplitList<LegionMember> legionMemberSplitList(std::move(members), true, 80);
		for (utils::collections::ListPart<LegionMember>& part : legionMemberSplitList) {
			if (broadcastToLegion)
				utils::PacketSendUtility::broadcastToLegion(*legion, SM_LEGION_MEMBERLIST(part.borrowed(), part.isFirst(), part.isLast()));
			else
				utils::PacketSendUtility::sendPacket(*player, SM_LEGION_MEMBERLIST(part.borrowed(), part.isFirst(), part.isLast()));
		}
	}
}

bool LegionService::tryRename(model::team::legion::Legion& legion, std::string_view name, model::gameobjects::player::Player& player, std::optional<int32_t> legionNameChangeTicketItemObjId) {
	AION_UNPORTED();
}

void LegionService::joinLegionDominion(model::gameobjects::player::Player& player, int32_t locId) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services
