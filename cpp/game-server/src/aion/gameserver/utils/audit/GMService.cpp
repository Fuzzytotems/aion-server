#include "aion/gameserver/utils/audit/GMService.h"

#include <algorithm>
#include <string>

#include "aion/commons/logging/Logger.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/configs/administration/AdminConfig.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/gameobjects/player/CustomPlayerState.h"
#include "aion/gameserver/model/gameobjects/player/FriendList.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/sched/Pin.h"
#include "aion/gameserver/services/SkillLearnService.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/utils/ChatUtil.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"

namespace aion::gameserver::utils::audit {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.utils.audit.GMService");

using model::gameobjects::player::Player;

GMService& GMService::getInstance() {
	static GMService instance; // Java: SingletonHolder
	return instance;
}

GMService::GMService() {
	for (const skillengine::model::SkillTemplate* t : dataholders::DataManager::SKILL_DATA->getSkillTemplates()) {
		// Java: t.getGroup() != null && t.getGroup().startsWith("GM_") || t.getStack().startsWith("GM_") (an absent group is empty)
		if (t->getGroup().starts_with("GM_") || t->getStack().starts_with("GM_"))
			gmSkills.add(t);
	}
	if (gmSkills.isEmpty())
		log.warn("No GM skills found, possibly because of changed or missing skill templates.");
}

std::vector<runtime::Ptr<Player>> GMService::getOnlineStaffMembers() {
	std::vector<runtime::Ptr<Player>> players;
	for (const runtime::Ref<Player>& player : staffMembers.values())
		players.emplace_back(player);
	return players;
}

std::vector<runtime::Ptr<Player>> GMService::getAvailableStaffMembers() {
	std::vector<runtime::Ptr<Player>> players = getOnlineStaffMembers();
	std::erase_if(players, [this](const runtime::Ptr<Player>& player) { return !isAvailable(*player); });
	return players;
}

void GMService::onPlayerLogin(Player& player) {
	if (player.isStaff()) {
		auto loginExecuteCommands = configs::administration::AdminConfig::LOGIN_EXECUTE_COMMANDS.get();
		if (!loginExecuteCommands->empty()) {
			// Java: AdminConfig.LOGIN_EXECUTE_COMMANDS.forEach(cmd -> ChatProcessor.getInstance().handleChatCommand(player, cmd));
			// utils/chathandlers/ChatProcessor.h (P5-14) does not exist yet
			AION_UNPORTED();
		}
		staffMembers.put(player.getObjectId(), runtime::Ref<Player>(player));
		scheduleBroadcastLogin(player);
	}
}

void GMService::onPlayerLogout(Player& player) {
	if (staffMembers.remove(player.getObjectId()) && isAnnounceable(player))
		broadcastConnectionStatus(player, false);
}

bool GMService::isAnnounceable(Player& player) {
	if (!player.isOnline() || !player.isStaff() || !isAvailable(player))
		return false;
	auto announceLevels = configs::administration::AdminConfig::ANNOUNCE_LEVELS.get();
	std::string accessLevel = std::to_string(player.getAccount()->getAccessLevel());
	return std::find(announceLevels->begin(), announceLevels->end(), accessLevel) != announceLevels->end() ||
		std::find(announceLevels->begin(), announceLevels->end(), "*") != announceLevels->end();
}

bool GMService::isAvailable(Player& player) {
	return !player.isInCustomState(model::gameobjects::player::CustomPlayerState::NO_WHISPERS_MODE) &&
		player.getFriendList().getStatus() != model::gameobjects::player::FriendList::Status::OFFLINE;
}

void GMService::broadcastConnectionStatus(Player& gm, bool connected) {
	using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
	std::string name = ChatUtil::charName(gm);
	SM_SYSTEM_MESSAGE sysMsg = connected ? SM_SYSTEM_MESSAGE::STR_NOTIFY_LOGIN_BUDDY(name) : SM_SYSTEM_MESSAGE::STR_NOTIFY_LOGOFF_BUDDY(name);
	if ((connected && configs::administration::AdminConfig::ANNOUNCE_LOGIN_TO_ALL_PLAYERS.load()) ||
		(!connected && configs::administration::AdminConfig::ANNOUNCE_LOGOUT_TO_ALL_PLAYERS.load())) {
		PacketSendUtility::broadcastToWorld(sysMsg, [&gm](Player& p) { return !p.equals(gm); });
	} else {
		PacketSendUtility::broadcastToWorld(sysMsg, [&gm](Player& p) { return p.isStaff() && !p.equals(gm); });
	}
}

// task lambda com.aionemu.gameserver.utils.audit.GMService@L91:44
void GMService::scheduleBroadcastLogin(Player& gm) {
	if (!isAnnounceable(gm))
		return;
	int8_t delay = 15;
	PacketSendUtility::sendMessage(gm, "Your login will be announced in " + std::to_string(delay) +
		"s.\nYou can disable this by setting whisper off or changing your online status to invisible.");
	ThreadPoolManager::getInstance().schedule(
		{this, &gm},
		[this, &gm] {
			if (isAnnounceable(gm)) {
				broadcastConnectionStatus(gm, true);
				PacketSendUtility::sendMessage(gm, "Your login has been announced.");
			} else {
				PacketSendUtility::sendMessage(gm, "Your login has not been announced.");
			}
		},
		delay * 1000);
}

void GMService::addGmSkills(Player& player) {
	for (const skillengine::model::SkillTemplate* t : gmSkills) {
		switch (t->getSkillId()) {
			case 322: // [Event] Manastone Preservation
			case 323: // Homerun Energy
			case 339: // Panesterra Dominant
				continue;
		}
		if (player.getRace() == model::Race::ASMODIANS && t->getStack().find("_LIGHT") != std::string::npos)
			continue;
		if (player.getRace() == model::Race::ELYOS && t->getStack().find("_DARK") != std::string::npos)
			continue;
		services::SkillLearnService::learnTemporarySkill(player, t->getSkillId(), t->getLvl());
	}
}

} // namespace aion::gameserver::utils::audit
