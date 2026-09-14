#include "aion/gameserver/services/ban/HDDBanService.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::services::ban {

HDDBanService::HDDBanService() = default;

HDDBanService::~HDDBanService() = default;

HDDBanService& HDDBanService::getInstance() {
	static HDDBanService instance; // Java SingletonHolder
	return instance;
}

void HDDBanService::addBan(std::string_view serial, std::optional<commons::database::Timestamp> banTime) {
	AION_UNPORTED();
}

void HDDBanService::removeBan(std::string_view serial) {
	AION_UNPORTED();
}

void HDDBanService::loadBan(std::string_view serial, int64_t banTime) {
	AION_UNPORTED();
}

bool HDDBanService::isBanned(std::string_view serial) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services::ban
