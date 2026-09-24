#include "aion/chatserver/utils/IdFactory.h"

namespace aion::chatserver::utils {

IdFactory& IdFactory::getInstance() {
	static auto* instance = new IdFactory(); // leaked: channels may be created while static objects are destroyed
	return *instance;
}

} // namespace aion::chatserver::utils
