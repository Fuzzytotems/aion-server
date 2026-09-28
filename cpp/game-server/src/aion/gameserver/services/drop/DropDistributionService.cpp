#include "aion/gameserver/services/drop/DropDistributionService.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::services::drop {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.drop.DropDistributionService");

DropDistributionService::DropDistributionService() = default;

DropDistributionService::~DropDistributionService() = default;

DropDistributionService& DropDistributionService::getInstance() {
	static DropDistributionService instance; // Java SingletonHolder
	return instance;
}

void DropDistributionService::handleRollOrBid(runtime::Ptr<model::gameobjects::player::Player> player, int32_t mode, int32_t roll, int64_t bid, int32_t itemId, int32_t npcObjId, int32_t index) {
	AION_UNPORTED();
}

void DropDistributionService::handleRoll(model::gameobjects::player::Player& player, int32_t roll, int32_t itemId, model::drop::DropItem& requestedItem, model::gameobjects::DropNpc& dropNpc) {
	AION_UNPORTED();
}

void DropDistributionService::handleBid(model::gameobjects::player::Player& player, int64_t bid, int32_t itemId, model::drop::DropItem& requestedItem, model::gameobjects::DropNpc& dropNpc) {
	AION_UNPORTED();
}

void DropDistributionService::distributeLoot(model::gameobjects::player::Player& player, int64_t luckyPlayer, int32_t itemId, model::drop::DropItem& requestedItem, model::gameobjects::DropNpc& dropNpc) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services::drop
