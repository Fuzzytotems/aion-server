#pragma once

#include <string>
#include <string_view>

#include "aion/gameserver/utils/fwd.h"

namespace aion::gameserver::utils {

/**
 * C++: a static-only class (fieldmap K5).
 *
 * @author -Nemesiss-
 */
class Util {
public:
	Util() = delete;

	/**
	 * Converts name to valid pattern For example : "atracer" -> "Atracer" (unchanged if NameConfig.ALLOW_CUSTOM_NAMES). Character indexes and case
	 * conversion follow Java's UTF-16 String rules (commons StringUtils).
	 */
	static std::string convertName(std::string_view name);
};

} // namespace aion::gameserver::utils
