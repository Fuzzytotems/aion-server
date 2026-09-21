#include "aion/gameserver/services/SurveyService.h"

#include <algorithm>
#include <string>
#include <vector>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/cache/HTMLCache.h"
#include "aion/gameserver/configs/main/SecurityConfig.h"
#include "aion/gameserver/dao/SurveyControllerDAO.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/ItemId.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/survey/SurveyItem.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/HTMLService.h"
#include "aion/gameserver/services/item/ItemService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/world/World.h"

namespace aion::gameserver::services {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.SurveyService");

namespace {

/** Java String.replace(CharSequence, CharSequence) */
std::string replaceAll(std::string text, std::string_view from, std::string_view to) {
	for (size_t at = text.find(from); at != std::string::npos; at = text.find(from, at + to.size()))
		text.replace(at, from.size(), to);
	return text;
}

} // namespace

// Java: public inner class TaskUpdate implements Runnable (calls taskUpdate() of the enclosing instance, the Immortal singleton). The scheduled
// lambda pins the singleton (a no-op for Immortals) and runs a TaskUpdate on it.
class SurveyService::TaskUpdate {
public:
	const SurveyService* surveyService; // Java: this$0 (always the singleton, so run() calls getInstance().taskUpdate())

	void run();
};

void SurveyService::TaskUpdate::run() {
	log.info("[SurveyController] update task start.");
	SurveyService::getInstance().taskUpdate();
}

SurveyService::SurveyService() {
	utils::ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] { TaskUpdate{this}.run(); }, 2000,
		int64_t{configs::main::SecurityConfig::SURVEY_DELAY.load()} * 60000);
}

bool SurveyService::isActive(model::gameobjects::player::Player& player, int32_t survId) {
	bool avail = activeItems.containsKey(survId);
	if (avail)
		requestSurvey(player, survId);

	return avail;
}

void SurveyService::requestSurvey(model::gameobjects::player::Player& player, int32_t survId) {
	runtime::Ptr<model::templates::survey::SurveyItem> item = activeItems.get(survId);
	if (!item || item->ownerId.get() != player.getObjectId()) {
		utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_CANNOT_FIND_POLL());
		return;
	}

	const model::templates::item::ItemTemplate* template_ = dataholders::DataManager::ITEM_DATA->getItemTemplate(item->itemId.get());
	if (template_ == nullptr) {
		return;
	}
	if (player.getInventory().isFull(template_->getExtraInventoryId())) {
		utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_FULL_INVENTORY());
		log.warn("[SurveyController] player " + player.getName() + " tried to receive item with full inventory.");
		return;
	}
	if (dao::SurveyControllerDAO::useItem(item->uniqueId.get())) {
		item::ItemService::addItem(player, item->itemId.get(), item->count.get());
		if (item->itemId.get() == model::items::ItemId::KINAH)
			utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_GET_POLL_REWARD_MONEY(item->count.get()));
		else if (item->count.get() == 1)
			utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_GET_POLL_REWARD_ITEM(template_->getL10n()));
		else
			utils::PacketSendUtility::sendPacket(player,
				network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_GET_POLL_REWARD_ITEM_MULTI(item->count.get(), template_->getL10n()));

		activeItems.remove(survId);
	}
}

void SurveyService::taskUpdate() {
	std::vector<runtime::Ref<model::templates::survey::SurveyItem>> newList = dao::SurveyControllerDAO::getAllUnused();
	if (newList.size() == 0)
		return;

	std::vector<int32_t> players;
	int32_t cnt = 0;
	for (const runtime::Ref<model::templates::survey::SurveyItem>& survey : newList) {
		if (!activeItems.putIfAbsent(survey->uniqueId.get(), survey)) {
			cnt++;
			if (std::ranges::find(players, survey->ownerId.get()) == players.end())
				players.push_back(survey->ownerId.get());
		}
	}
	log.info("[SurveyController] found new " + std::to_string(cnt) + " items for " + std::to_string(players.size()) + " players.");
	for (int32_t ownerId : players) {
		runtime::Ptr<model::gameobjects::player::Player> player = world::World::getInstance().getPlayer(ownerId);
		if (player) {
			showAvailable(*player);
		}
	}
}

void SurveyService::showAvailable(model::gameobjects::player::Player& player) {
	for (const runtime::Ptr<model::templates::survey::SurveyItem>& item : activeItems.values()) {
		if (item->ownerId.get() != player.getObjectId())
			continue;

		std::optional<std::string> html = cache::HTMLCache::getInstance().getHTML("surveyTemplate.xhtml");
		if (!html) // Java: context.replace(...) on a null template
			throw runtime::NullPointerException("HTML template surveyTemplate.xhtml is null");
		std::string context = std::move(*html);
		context = replaceAll(std::move(context), "%itemid%", std::to_string(item->itemId.get()));
		context = replaceAll(std::move(context), "%itemcount%", std::to_string(item->count.get()));
		context = replaceAll(std::move(context), "%html%", item->html.get());
		context = replaceAll(std::move(context), "%radio%", item->radio.get());

		HTMLService::sendData(player, item->uniqueId.get(), context);
	}
}

SurveyService& SurveyService::getInstance() {
	static SurveyService instance; // Java SingletonHolder
	return instance;
}

} // namespace aion::gameserver::services
