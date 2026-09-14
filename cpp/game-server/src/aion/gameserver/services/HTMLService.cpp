#include "aion/gameserver/services/HTMLService.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::services {

static const auto log = commons::logging::LoggerFactory::getLogger("ITEM_HTML_LOG");

std::string HTMLService::getHTMLTemplate(const model::templates::Guides::GuideTemplate* template_) {
	AION_UNPORTED();
}

void HTMLService::pushSurvey(std::string_view html) {
	AION_UNPORTED();
}

void HTMLService::showHTML(model::gameobjects::player::Player& player, std::string_view html) {
	AION_UNPORTED();
}

void HTMLService::sendData(model::gameobjects::player::Player& player, int32_t messageId, std::string_view html) {
	AION_UNPORTED();
}

void HTMLService::sendGuideHtml(model::gameobjects::player::Player& player, int32_t fromLevel, int32_t toLevel) {
	AION_UNPORTED();
}

void HTMLService::onPlayerLogin(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void HTMLService::getReward(runtime::Ptr<model::gameobjects::player::Player> player, int32_t messageId, const std::vector<int32_t>& items) {
	AION_UNPORTED();
}

std::vector<const model::templates::Guides::SurveyTemplate*> HTMLService::getSurveyTemplates(const std::vector<const model::templates::Guides::SurveyTemplate*>& surveys, const std::vector<int32_t>& items) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services
