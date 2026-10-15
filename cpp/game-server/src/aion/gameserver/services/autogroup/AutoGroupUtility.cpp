#include "aion/gameserver/services/autogroup/AutoGroupUtility.h"

#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/InstanceCooltimeData.h"
#include "aion/gameserver/model/autogroup/AGPlayer.h"
#include "aion/gameserver/model/autogroup/AutoGroup.h"
#include "aion/gameserver/model/autogroup/AutoGroupTypeInfo.h"
#include "aion/gameserver/model/autogroup/EntryRequestTypeInfo.h"
#include "aion/gameserver/model/autogroup/LookingForParty.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PortalCooldownList.h"
#include "aion/gameserver/model/team/TemporaryPlayerTeam.h"
#include "aion/gameserver/network/aion/serverpackets/SM_AUTO_GROUP.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/AutoGroupService.h"
#include "aion/gameserver/services/instance/PvPArenaService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/world/World.h"

namespace aion::gameserver::services::autogroup {

using model::autogroup::AutoGroup;
using model::gameobjects::player::Player;
using network::aion::serverpackets::SM_AUTO_GROUP;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using utils::PacketSendUtility;

/** Java: agt.getTemplate(), dereferenced by every caller (a missing template is Java's NullPointerException) */
struct AutoGroupUtility_Template {
	static const AutoGroup& of(model::autogroup::AutoGroupType agt) {
		const AutoGroup* found = model::autogroup::getTemplate(agt);
		if (found == nullptr)
			throw runtime::NullPointerException("AutoGroupType without auto group template");
		return *found;
	}
};

// Java AutoGroupUtility.java:20-28
bool AutoGroupUtility::canRegisterNewEntry(Player& player, model::autogroup::AutoGroupType agt) {
	if (!AutoGroupUtility_Template::of(agt).canRegisterNewEntry())
		return false;
	if (player.isInTeam()) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_CANT_INSTANCE_NOT_LEADER());
		return false;
	}
	return true;
}

// Java AutoGroupUtility.java:30-38
bool AutoGroupUtility::canRegisterQuickEntry(Player& player, model::autogroup::AutoGroupType agt) {
	if (!AutoGroupUtility_Template::of(agt).canRegisterQuickEntry())
		return false;
	if (player.isInTeam()) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_CANT_INSTANCE_NOT_LEADER());
		return false;
	}
	return true;
}

// Java AutoGroupUtility.java:40-42
bool AutoGroupUtility::canRegisterGroupEntry(Player& player, model::autogroup::AutoGroupType agt, int32_t mapId, int32_t maskId) {
	return AutoGroupUtility_Template::of(agt).hasRegisterGroup() && checkGroupRequirements(player, agt, mapId, maskId);
}

// Java AutoGroupUtility.java:44-78
bool AutoGroupUtility::checkGroupRequirements(Player& player, model::autogroup::AutoGroupType agt, int32_t mapId, int32_t maskId) {
	runtime::Ptr<model::team::TemporaryPlayerTeam> team = player.getCurrentTeam();
	if (team == nullptr || !team->isLeader(player)) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_CANT_INSTANCE_NOT_LEADER());
		return false;
	}
	if (model::autogroup::isPeriodicInstance(agt)) {
		int32_t maxMemberPerTeam = dataholders::DataManager::INSTANCE_COOLTIME_DATA->getMaxMemberCount(
			AutoGroupUtility_Template::of(agt).getInstanceMapId(), player.getRace());
		if (team->size() > maxMemberPerTeam) {
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_CANT_INSTANCE_TOO_MANY_MEMBERS(maxMemberPerTeam, mapId));
			return false;
		}
	} else if (model::autogroup::isHarmonyArena(agt) || model::autogroup::isTrainingHarmonyArena(agt)) {
		if (team->size() > 3) {
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_CANT_INSTANCE_TOO_MANY_MEMBERS(3, mapId));
			return false;
		}
	}
	for (const runtime::Ptr<model::gameobjects::AionObject>& object : team->getMembers()) {
		Player& member = *runtime::cast<Player>(object);
		if (team->getLeaderObject()->equals(member)) {
			continue;
		}
		if (model::autogroup::isHarmonyArena(agt) && !instance::PvPArenaService::checkItem(member, agt)) {
			PacketSendUtility::sendPacket(member, SM_SYSTEM_MESSAGE::STR_MSG_INSTANCE_CANT_ENTER_WITHOUT_ITEM());
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_CANT_INSTANCE_ENTER_MEMBER(member.getName()));
			return false;
		}
		if (hasCoolDown(member, mapId) || !model::autogroup::isInLvlRange(agt, member.getLevel()) ||
			AutoGroupService::getInstance().isSearching(member, maskId)) {
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_CANT_INSTANCE_ENTER_MEMBER(member.getName()));
			return false;
		}
	}
	return true;
}

// Java AutoGroupUtility.java:80-90
void AutoGroupUtility::sendSuccessfulRegistration(model::autogroup::LookingForParty& lfp, std::string_view leaderName,
	model::autogroup::AutoGroupType agt, int32_t maskId) {
	for (int32_t objectId : lfp.getMembers().keySet()) {
		runtime::Ptr<Player> player = world::World::getInstance().getPlayer(objectId);
		if (player != nullptr) {
			if (model::autogroup::isPeriodicInstance(agt))
				PacketSendUtility::sendPacket(*player, SM_AUTO_GROUP(maskId, 6, true));
			PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_MSG_INSTANCE_REGISTER_SUCCESS());
			PacketSendUtility::sendPacket(*player, SM_AUTO_GROUP(maskId, 1, model::autogroup::getId(lfp.getEntryRequestType()), leaderName));
		}
	}
}

// Java AutoGroupUtility.java:92-96
void AutoGroupUtility::sendWindowToPlayerIfOnline(int32_t objectId, int32_t maskId, int32_t windowId) {
	runtime::Ptr<Player> player = world::World::getInstance().getPlayer(objectId);
	if (player != nullptr)
		sendWindowToPlayer(*player, maskId, windowId);
}

// Java AutoGroupUtility.java:98-100
void AutoGroupUtility::sendWindowToPlayer(Player& player, int32_t maskId, int32_t windowId) {
	PacketSendUtility::sendPacket(player, SM_AUTO_GROUP(maskId, windowId));
}

// Java AutoGroupUtility.java:102-104
bool AutoGroupUtility::hasCoolDown(Player& player, int32_t worldId) {
	return player.getPortalCooldownList().isPortalUseDisabled(worldId);
}

} // namespace aion::gameserver::services::autogroup
