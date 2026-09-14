#include "aion/gameserver/services/AdminService.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::services {

static const auto itemLog = commons::logging::LoggerFactory::getLogger("GMITEMRESTRICTION");

AdminService::AdminService() {
	AION_UNPORTED();
}

AdminService::~AdminService() = default;

AdminService& AdminService::getInstance() {
	static AdminService instance; // Java SingletonHolder
	return instance;
}

void AdminService::reload() {
	AION_UNPORTED();
}

bool AdminService::canOperate(model::gameobjects::player::Player& player, runtime::Ptr<model::gameobjects::player::Player> target, model::gameobjects::Item& item, std::string_view type) {
	AION_UNPORTED();
}

bool AdminService::canOperate(model::gameobjects::player::Player& player, runtime::Ptr<model::gameobjects::player::Player> target, int32_t itemId, std::string_view type) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services
