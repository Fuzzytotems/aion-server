#include "aion/gameserver/dao/ServerVariablesDAO.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::dao {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.ServerVariablesDAO");

std::optional<int32_t> ServerVariablesDAO::loadInt(std::string_view var) {
	AION_UNPORTED();
}

std::optional<int64_t> ServerVariablesDAO::loadLong(std::string_view var) {
	AION_UNPORTED();
}

bool ServerVariablesDAO::store(std::string_view var, const std::any& value) {
	AION_UNPORTED();
}

bool ServerVariablesDAO::delete_(std::string_view var) {
	AION_UNPORTED();
}

std::optional<std::string> ServerVariablesDAO::load(std::string_view var) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::dao
