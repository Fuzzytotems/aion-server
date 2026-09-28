#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/templates/Guides/fwd.h"
#include "aion/gameserver/services/fwd.h"

namespace aion::gameserver::services {

/**
 * Use this service to send raw html to the client.
 * <p>
 * C++: a static-only class (hub-headers.md §11.1).
 *
 * @author lhw, xTz
 */
class HTMLService {
public:
	static std::string getHTMLTemplate(const model::templates::Guides::GuideTemplate* template_);
	static void pushSurvey(std::string_view html);
	static void showHTML(model::gameobjects::player::Player& player, std::string_view html);
	static void sendData(model::gameobjects::player::Player& player, int32_t messageId, std::string_view html);
	static void sendGuideHtml(model::gameobjects::player::Player& player, int32_t fromLevel, int32_t toLevel);
	static void onPlayerLogin(model::gameobjects::player::Player& player);
	static void getReward(runtime::Ptr<model::gameobjects::player::Player> player, int32_t messageId, const std::vector<int32_t>& items);
private:
	static std::vector<const model::templates::Guides::SurveyTemplate*> getSurveyTemplates(const std::vector<const model::templates::Guides::SurveyTemplate*>& surveys, const std::vector<int32_t>& items);
};

} // namespace aion::gameserver::services
