#include "aion/gameserver/services/abyss/AbyssPointsService.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::services::abyss {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.abyss.AbyssPointsService");

// callback at AbyssPointsService.java:34 (fieldmap key AbyssPointsService@L34:25)
void AbyssPointsService::addAp(model::gameobjects::player::Player& player, model::gameobjects::VisibleObject& obj, int32_t value) {
	AION_UNPORTED();
}

void AbyssPointsService::addAp(model::gameobjects::player::Player& player, int32_t amount) {
	AION_UNPORTED();
}

void AbyssPointsService::addAp(runtime::Ptr<model::gameobjects::player::Player> player, int32_t amount, const std::function<network::aion::serverpackets::SM_SYSTEM_MESSAGE(int32_t)>& gainMessage) {
	AION_UNPORTED();
}

void AbyssPointsService::onRankChanged(model::gameobjects::player::Player& player, bool abyssPointChanged, bool abyssRankChanged, std::optional<int32_t> newRankingListPosition) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services::abyss
