#include "aion/gameserver/model/gameobjects/player/title/TitleList.h"

#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/dao/PlayerTitleListDAO.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/TitleData.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/player/title/Title.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/model/templates/TitleTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_TITLE_INFO.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::model::gameobjects::player::title {

namespace {

/** Java ExpireTimerTask.getInstance().registerExpirable(expirable, player); the task manager (P5-14) has no C++ header yet */
void registerExpirable(Title& expirable, Player& player) {
	static_cast<void>(expirable);
	static_cast<void>(player);
	AION_UNPORTED();
}

} // namespace

TitleList::TitleList() = default;

TitleList::TitleList(Player& partOwner) : OwnedPart(partOwner) {
}

TitleList::~TitleList() = default;

void TitleList::setOwner(Player& value) {
	if (!isOwnerBound())
		bindOwner(value);
	owner.set(&value);
}

runtime::Ptr<Player> TitleList::getOwner() const {
	return runtime::Ptr<Player>(owner.get());
}

bool TitleList::contains(int32_t titleId) {
	return titles.containsKey(titleId);
}

void TitleList::addEntry(int32_t titleId, int32_t remaining) {
	const templates::TitleTemplate* tt = dataholders::DataManager::TITLE_DATA->getTitleTemplate(titleId);
	if (tt == nullptr)
		throw runtime::IllegalArgumentException("Invalid title id " + std::to_string(titleId));
	titles.put(titleId, Title::create(tt, titleId, remaining));
}

bool TitleList::addTitle(int32_t titleId, bool questReward, int32_t time) {
	const templates::TitleTemplate* tt = dataholders::DataManager::TITLE_DATA->getTitleTemplate(titleId);
	if (tt == nullptr)
		throw runtime::IllegalArgumentException("Invalid title id " + std::to_string(titleId));
	if (runtime::Ptr<Player> player = getOwner()) {
		if (player->getRace() != tt->getRace() && tt->getRace() != Race::PC_ALL) {
			utils::PacketSendUtility::sendMessage(*player, "This title is not available for your race.");
			return false;
		}
		runtime::Ref<Title> entry = Title::create(tt, titleId, time);
		if (!titles.containsKey(titleId)) {
			titles.put(titleId, entry);
			registerExpirable(*entry, *player);
			dao::PlayerTitleListDAO::storeTitles(*player, *entry);
		} else {
			utils::PacketSendUtility::sendPacket(*player, network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_TOOLTIP_LEARNED_TITLE());
			return false;
		}
		if (questReward)
			utils::PacketSendUtility::sendPacket(*player, network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_QUEST_GET_REWARD_TITLE(tt->getL10n()));
		else
			utils::PacketSendUtility::sendPacket(*player, network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_GET_CASH_TITLE(tt->getL10n()));

		utils::PacketSendUtility::sendPacket(*player, network::aion::serverpackets::SM_TITLE_INFO(*player));
		return true;
	}
	return false;
}

void TitleList::setDisplayTitle(int32_t titleId) {
	Player& player = *getOwner();
	utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_TITLE_INFO(titleId));
	utils::PacketSendUtility::broadcastPacketAndReceive(player, network::aion::serverpackets::SM_TITLE_INFO(player, titleId));
	player.getCommonData()->setTitleId(titleId);
	player.getController().updateNearbyQuests();
}

void TitleList::setBonusTitle(int32_t bonusTitleId) {
	Player& player = *getOwner();
	utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_TITLE_INFO(6, bonusTitleId));
	if (player.getCommonData()->getBonusTitleId() > 0) {
		if (player.getGameStats()) {
			// Java: TitleChangeListener.onBonusTitleChange(owner.getGameStats(), owner.getCommonData().getBonusTitleId(), false); the listener
			// (P5-01) has no C++ header yet
			AION_UNPORTED();
		}
	}
	player.getCommonData()->setBonusTitleId(bonusTitleId);
	if (bonusTitleId > 0 && player.getGameStats()) {
		// Java: TitleChangeListener.onBonusTitleChange(owner.getGameStats(), bonusTitleId, true) (P5-01, no C++ header yet)
		AION_UNPORTED();
	}
}

void TitleList::removeTitle(int32_t titleId) {
	if (!titles.containsKey(titleId))
		return;
	Player& player = *getOwner();
	if (player.getCommonData()->getTitleId() == titleId)
		setDisplayTitle(-1);
	if (player.getCommonData()->getBonusTitleId() == titleId)
		setBonusTitle(-1);
	titles.remove(titleId);
	utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_TITLE_INFO(player));
	dao::PlayerTitleListDAO::removeTitle(player.getObjectId(), titleId);
}

int32_t TitleList::size() {
	return titles.size();
}

std::vector<runtime::Ptr<Title>> TitleList::getTitles() {
	return titles.values();
}

} // namespace aion::gameserver::model::gameobjects::player::title
