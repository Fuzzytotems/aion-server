#include "aion/gameserver/services/AnnouncementService.h"

#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/dao/AnnouncementsDAO.h"
#include "aion/gameserver/model/Announcement.h"
#include "aion/gameserver/model/ChatType.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MESSAGE.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"

namespace aion::gameserver::services {

AnnouncementService::AnnouncementService() {
	reload();
}

AnnouncementService::~AnnouncementService() = default;

AnnouncementService& AnnouncementService::getInstance() {
	static AnnouncementService instance; // Java SingletonHolder
	return instance;
}

void AnnouncementService::reload() {
	// Cancel all tasks
	for (const runtime::Ptr<runtime::Future>& delay : delays.values())
		delay->cancel(true);
	delays.clear();
	announcements.clear();

	// And load again all announcements
	for (const runtime::Ref<model::Announcement>& announcement : dao::AnnouncementsDAO::loadAnnouncements())
		schedule(*announcement);

	commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.AnnouncementService")
		.info("Loaded " + std::to_string(announcements.size()) + " announcements");
}

// anonymous Runnable at AnnouncementService.java:55 (fieldmap key AnnouncementService$1); argument 1 of scheduleAtFixedRate(); storage: task in
// AnnouncementService. It captures only the announcement (a Ref init-capture, nothing pinned).
void AnnouncementService::schedule(model::Announcement& announce) {
	announcements.put(announce.getId(), runtime::Ref<model::Announcement>(announce));
	delays.put(announce.getId(),
		utils::ThreadPoolManager::getInstance().scheduleAtFixedRate(runtime::Pin(),
			[announce = runtime::Ref<model::Announcement>(announce)] {
				std::string sender = !announce->getFaction()						 ? ""
					: announce->getFaction() == gameserver::model::Race::ELYOS ? "Elyos "
																				 : "Asmodian ";
				sender += "Announcement";
				std::string msg = announce->getChatType() == gameserver::model::ChatType::SHOUT ||
						announce->getChatType() == gameserver::model::ChatType::GROUP_LEADER
					? ""
					: sender + ": ";
				msg += announce->getAnnounce();
				network::aion::serverpackets::SM_MESSAGE message(1, sender, msg, announce->getChatType());
				utils::PacketSendUtility::broadcastToWorld(message, [&announce](model::gameobjects::player::Player& player) {
					return announce->getFaction() != player.getOppositeRace();
				});
			},
			int64_t{announce.getDelay()} * 1000, int64_t{announce.getDelay()} * 1000));
}

bool AnnouncementService::addAnnouncement(std::string_view message, std::string_view faction, std::string_view chatType, int32_t delay) {
	int32_t id = dao::AnnouncementsDAO::addAnnouncement(message, faction, chatType, delay);
	if (id == -1)
		return false;
	schedule(*model::Announcement::create(id, message, faction, chatType, delay));
	return true;
}

bool AnnouncementService::delAnnouncement(int32_t id) {
	if (!announcements.remove(id) || !dao::AnnouncementsDAO::delAnnouncement(id))
		return false;
	runtime::Ptr<runtime::Future> delay = delays.remove(id);
	if (delay)
		delay->cancel(false);
	return true;
}

std::vector<runtime::Ptr<model::Announcement>> AnnouncementService::getAnnouncements() {
	std::vector<runtime::Ptr<model::Announcement>> list;
	for (const runtime::Ptr<model::Announcement>& announcement : announcements.values())
		list.push_back(announcement);
	return list;
}

} // namespace aion::gameserver::services
