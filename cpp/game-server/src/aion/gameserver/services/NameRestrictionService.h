#pragma once

#include <string>
#include <string_view>

#include "aion/gameserver/services/fwd.h"

namespace aion::gameserver::services {

/**
 * @author nrg, Neon
 */
class NameRestrictionService {
public:
	static bool isValidName(std::string_view name);
	static bool isValidPetName(std::string_view name);
	static bool isValidLegionName(std::string_view name);
	static bool isForbidden(std::string_view name);
private:
	static bool containsForbiddenSequence(std::string_view name);
public:
	static bool isForbiddenWord(std::string_view string);
	static std::string filterMessage(std::string_view message);
};

} // namespace aion::gameserver::services
