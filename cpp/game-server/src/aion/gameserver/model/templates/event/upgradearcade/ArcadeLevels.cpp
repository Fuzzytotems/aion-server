#include "aion/gameserver/model/templates/event/upgradearcade/ArcadeLevels.h"

#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::model::templates::event::upgradearcade {

const ArcadeLevel* ArcadeLevels::getMaxUpgradeLevel() const {
	if (upgradeLevels.empty())
		throw runtime::IndexOutOfBoundsException("Index -1 out of bounds for length 0");
	return &upgradeLevels.back();
}

} // namespace aion::gameserver::model::templates::event::upgradearcade
