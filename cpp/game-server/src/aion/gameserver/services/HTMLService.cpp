#include "aion/gameserver/services/HTMLService.h"

#include <algorithm>
#include <cstdint>
#include <stacktrace>
#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/cache/HTMLCache.h"
#include "aion/gameserver/configs/main/LoggingConfig.h"
#include "aion/gameserver/dao/GuideDAO.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/GuideHtmlData.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/guide/Guide.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/Guides/GuideTemplate.h"
#include "aion/gameserver/model/templates/Guides/SurveyTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUESTIONNAIRE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/SurveyService.h"
#include "aion/gameserver/services/item/ItemService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"
#include "aion/gameserver/world/World.h"

namespace aion::gameserver::services {

static const auto log = commons::logging::LoggerFactory::getLogger("ITEM_HTML_LOG");

namespace {

using commons::utils::StringUtils::substring;
using commons::utils::StringUtils::utf16Length;

/** Java String.replace(CharSequence, CharSequence) */
std::string replaceAll(std::string text, std::string_view from, std::string_view to) {
	for (size_t at = text.find(from); at != std::string::npos; at = text.find(from, at + to.size()))
		text.replace(at, from.size(), to);
	return text;
}

/** HTMLCache.getInstance().getHTML(name), which Java then dereferences */
std::string htmlTemplate(std::string_view name) {
	std::optional<std::string> html = cache::HTMLCache::getInstance().getHTML(name);
	if (!html)
		throw runtime::NullPointerException("HTML template " + std::string(name) + " is null");
	return std::move(*html);
}

} // namespace

std::string HTMLService::getHTMLTemplate(const model::templates::Guides::GuideTemplate* template_) {
	std::string context = htmlTemplate("guideTemplate.xhtml");

	std::string sb;
	sb.append("<reward_items multi_count='").append(std::to_string(template_->getRewardCount())).append("'>\n");
	for (const model::templates::Guides::SurveyTemplate& survey : template_->getSurveys()) {
		sb.append("<item_id count='").append(std::to_string(survey.getCount())).append("'>").append(std::to_string(survey.getItemId())).append("</item_id>\n");
	}
	sb.append("</reward_items>\n");
	context = replaceAll(std::move(context), "%reward%", sb);
	context = replaceAll(std::move(context), "%radio%", template_->getSelect().empty() ? " " : template_->getSelect());
	context = replaceAll(std::move(context), "%html%", template_->getMessage().empty() ? " " : template_->getMessage());
	context = replaceAll(std::move(context), "%rewardInfo%", template_->getRewardInfo().empty() ? " " : template_->getRewardInfo());
	return context;
}

void HTMLService::pushSurvey(std::string_view html) {
	int32_t messageId = utils::idfactory::IDFactory::getInstance().nextId();
	std::string text(html);
	world::World::getInstance().forEachPlayer([messageId, &text](model::gameobjects::player::Player& player) { sendData(player, messageId, text); });
}

void HTMLService::showHTML(model::gameobjects::player::Player& player, std::string_view html) {
	sendData(player, utils::idfactory::IDFactory::getInstance().nextId(), html);
}

void HTMLService::sendData(model::gameobjects::player::Player& player, int32_t messageId, std::string_view html) {
	int32_t length = utf16Length(html);
	int32_t packetCount = static_cast<int32_t>(static_cast<float>(length) / (INT16_MAX - 8.0f)) + 1;
	if (packetCount > 255) { // max byte number (0xFF)
		// attach throwable for stacktrace
		log.warn("HTML message could not be sent to client, since its content is too long\n" + std::to_string(std::stacktrace::current()));
		return;
	}
	for (int32_t partNo = 0; partNo < packetCount; partNo++) {
		try {
			int32_t from = std::max(0, partNo * (INT16_MAX - 8));
			int32_t to = std::min(length, (partNo + 1) * (INT16_MAX - 8));
			utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_QUESTIONNAIRE(messageId, static_cast<int8_t>(partNo),
				static_cast<int8_t>(packetCount), substring(html, from, to)));
		} catch (const std::exception&) {
			log.errorCurrentException("htmlservice.sendData");
		}
	}
}

void HTMLService::sendGuideHtml(model::gameobjects::player::Player& player, int32_t fromLevel, int32_t toLevel) {
	for (int32_t level = fromLevel; level <= toLevel; level++) {
		std::vector<const model::templates::Guides::GuideTemplate*> surveyTemplate =
			dataholders::DataManager::GUIDE_HTML_DATA->getTemplatesFor(player.getPlayerClass(), player.getRace(), level);

		for (const model::templates::Guides::GuideTemplate* template_ : surveyTemplate) {
			if (!template_->isActivated())
				continue;
			int32_t id = utils::idfactory::IDFactory::getInstance().nextId();
			sendData(player, id, getHTMLTemplate(template_));
			dao::GuideDAO::saveGuide(id, player, template_->getTitle());
		}
	}
}

void HTMLService::onPlayerLogin(model::gameobjects::player::Player& player) {
	std::vector<model::guide::Guide> guides = dao::GuideDAO::loadGuides(player.getObjectId());

	for (const model::guide::Guide& guide : guides) {
		const model::templates::Guides::GuideTemplate* template_ = dataholders::DataManager::GUIDE_HTML_DATA->getTemplateByTitle(guide.getTitle());
		if (template_ != nullptr) {
			if (template_->isActivated())
				sendData(player, guide.getGuideId(), getHTMLTemplate(template_));
		} else {
			log.warn("Null guide template for title: " + guide.getTitle());
		}
	}
}

void HTMLService::getReward(runtime::Ptr<model::gameobjects::player::Player> player, int32_t messageId, const std::vector<int32_t>& items) {
	if (!player || messageId < 1) {
		return;
	}

	if (SurveyService::getInstance().isActive(*player, messageId)) {
		return;
	}

	std::optional<model::guide::Guide> guide = dao::GuideDAO::loadGuide(player->getObjectId(), messageId);

	if (guide) {
		const model::templates::Guides::GuideTemplate* template_ = dataholders::DataManager::GUIDE_HTML_DATA->getTemplateByTitle(guide->getTitle());
		if (template_ == nullptr) {
			return;
		}

		if (static_cast<int32_t>(items.size()) > template_->getRewardCount()) {
			return;
		}

		if (static_cast<int32_t>(items.size()) > player->getInventory().getFreeSlots()) {
			utils::PacketSendUtility::sendPacket(*player, network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_DICE_INVEN_ERROR());
			return;
		}
		std::vector<const model::templates::Guides::SurveyTemplate*> surveys;
		for (const model::templates::Guides::SurveyTemplate& survey : template_->getSurveys())
			surveys.push_back(&survey);
		std::vector<const model::templates::Guides::SurveyTemplate*> templates;
		if (static_cast<int32_t>(template_->getSurveys().size()) != template_->getRewardCount()) {
			templates = getSurveyTemplates(surveys, items);
		} else {
			templates = surveys;
		}
		if (templates.empty()) {
			return;
		}
		for (const model::templates::Guides::SurveyTemplate* item : templates) {
			item::ItemService::addItem(*player, item->getItemId(), item->getCount());
			if (configs::main::LoggingConfig::LOG_ITEM.load()) {
				log.info("[ITEM] Item Guide ID/Count - " + std::to_string(item->getItemId()) + "/" + std::to_string(item->getCount()) + " to player " +
					player->getName() + ".");
			}
		}
		dao::GuideDAO::deleteGuide(guide->getGuideId());
		utils::idfactory::IDFactory::getInstance().releaseId(guide->getGuideId());
		// Java: items.clear() empties the caller's list; the parameter is a const view here and its only caller (CM_QUESTIONNAIRE) drops it
	}
}

std::vector<const model::templates::Guides::SurveyTemplate*> HTMLService::getSurveyTemplates(
	const std::vector<const model::templates::Guides::SurveyTemplate*>& surveys, const std::vector<int32_t>& items) {
	std::vector<const model::templates::Guides::SurveyTemplate*> templates;
	for (const model::templates::Guides::SurveyTemplate* survey : surveys) {
		if (std::ranges::find(items, survey->getItemId()) != items.end()) {
			templates.push_back(survey);
		}
	}
	return templates;
}

} // namespace aion::gameserver::services
