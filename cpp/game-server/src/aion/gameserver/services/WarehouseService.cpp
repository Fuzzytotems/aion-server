#include "aion/gameserver/services/WarehouseService.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::services {

// Anonymous classes and stored lambdas of the Java class (hub-headers.md §7.3): the bodies that create them define the structs that
// `python tools/gen/fieldmap.py --class <key>` prints.
// anonymous RequestResponseHandler at WarehouseService.java:54 (com.aionemu.gameserver.services.WarehouseService$1); local responseHandler; storage:
// stored in ResponseRequester

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.WarehouseService");

void WarehouseService::expandWarehouse(model::gameobjects::player::Player& player, model::gameobjects::Npc& npc) {
	AION_UNPORTED();
}

void WarehouseService::expand(model::gameobjects::player::Player& player, bool isNpcExpand) {
	AION_UNPORTED();
}

bool WarehouseService::canExpandByTicket(model::gameobjects::player::Player& player, int32_t ticketLevel) {
	AION_UNPORTED();
}

bool WarehouseService::canExpand(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

int32_t WarehouseService::getCompletedWhQuests(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void WarehouseService::sendWarehouseInfo(model::gameobjects::player::Player& player, bool sendAccountWh) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services
