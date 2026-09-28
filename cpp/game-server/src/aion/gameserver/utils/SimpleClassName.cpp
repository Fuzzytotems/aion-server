#include "aion/gameserver/utils/SimpleClassName.h"

#include "aion/commons/utils/ClassName.h"

namespace aion::gameserver::utils {

std::string simpleClassName(const std::type_info& type) {
	std::string name = commons::utils::getSimpleClassName(type);
	if (size_t templateArguments = name.find('<'); templateArguments != std::string::npos && templateArguments > 0)
		name.erase(templateArguments); // "AITemplate<class aion::...::Npc>" -> "AITemplate" (a closure name "<lambda_1>" starts with '<' and stays)
	return name;
}

} // namespace aion::gameserver::utils
