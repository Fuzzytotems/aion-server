#include "aion/gameserver/world/zone/handler/GeneralZoneHandler.h"

namespace aion::gameserver::world::zone::handler {

GeneralZoneHandler::~GeneralZoneHandler() = default;

runtime::Ref<GeneralZoneHandler> GeneralZoneHandler::create() {
	return runtime::makeRef<GeneralZoneHandler>();
}

} // namespace aion::gameserver::world::zone::handler
