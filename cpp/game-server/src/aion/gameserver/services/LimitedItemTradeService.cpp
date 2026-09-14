#include "aion/gameserver/services/LimitedItemTradeService.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/model/limiteditems/LimitedTradeNpc.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::services {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.LimitedItemTradeService");

LimitedItemTradeService::LimitedItemTradeService() = default;

LimitedItemTradeService::~LimitedItemTradeService() = default;

LimitedItemTradeService& LimitedItemTradeService::getInstance() {
	static LimitedItemTradeService instance; // Java SingletonHolder
	return instance;
}

// callback at LimitedItemTradeService.java:46 (fieldmap key LimitedItemTradeService@L46:40)
void LimitedItemTradeService::start() {
	AION_UNPORTED();
}

runtime::Ptr<model::limiteditems::LimitedItem> LimitedItemTradeService::getLimitedItem(int32_t itemId, int32_t npcId) {
	AION_UNPORTED();
}

bool LimitedItemTradeService::isLimitedTradeNpc(int32_t npcId) {
	AION_UNPORTED();
}

runtime::Ptr<model::limiteditems::LimitedTradeNpc> LimitedItemTradeService::getLimitedTradeNpc(int32_t npcId) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services
