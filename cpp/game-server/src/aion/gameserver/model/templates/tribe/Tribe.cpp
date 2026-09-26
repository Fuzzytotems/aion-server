#include "aion/gameserver/model/templates/tribe/Tribe.h"

#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/model/TribeClassInfo.h"

namespace aion::gameserver::model::templates::tribe {

bool Tribe::isGuard() const {
	return model::isGuard(name);
}

std::string Tribe::toString() const {
	return std::string(xml::enumName(name)) + " (" + std::string(xml::enumName(base)) + ")";
}

const std::vector<TribeClass>& Tribe::listOrEmpty(const std::optional<std::vector<TribeClass>>& list) {
	static const std::vector<TribeClass> empty;
	return list ? *list : empty;
}

} // namespace aion::gameserver::model::templates::tribe
