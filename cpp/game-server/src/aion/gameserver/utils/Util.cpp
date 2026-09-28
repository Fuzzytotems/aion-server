#include "aion/gameserver/utils/Util.h"

#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/configs/main/NameConfig.h"

namespace aion::gameserver::utils {

std::string Util::convertName(std::string_view name) {
	namespace StringUtils = commons::utils::StringUtils;
	if (!name.empty()) {
		if (configs::main::NameConfig::ALLOW_CUSTOM_NAMES.load())
			return std::string(name);
		return StringUtils::toUpperCase(StringUtils::substring(name, 0, 1)) + StringUtils::substring(StringUtils::toLowerCase(name), 1);
	}
	return "";
}

} // namespace aion::gameserver::utils
