#include "aion/gameserver/dataholders/UpgradeArcadeData.h"

#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::dataholders {

using model::templates::event::upgradearcade::ArcadeLevel;
using model::templates::event::upgradearcade::ArcadeLevels;

namespace {

const ArcadeLevels& levelsOf(const std::unique_ptr<ArcadeLevels>& levels) {
	if (levels == nullptr)
		throw runtime::NullPointerException("Cannot invoke \"ArcadeLevels.getLevels()\" because \"this.levels\" is null");
	return *levels;
}

} // namespace

int32_t UpgradeArcadeData::size() const {
	return static_cast<int32_t>(rewards.size());
}

int32_t UpgradeArcadeData::getMinResumableLevel() const {
	return levelsOf(levels).getMinResumableLevel();
}

const std::vector<ArcadeLevel>& UpgradeArcadeData::getUpgradeLevels() const {
	return levelsOf(levels).getLevels();
}

const ArcadeLevel* UpgradeArcadeData::getMaxUpgradeLevel() const {
	return levelsOf(levels).getMaxUpgradeLevel();
}

} // namespace aion::gameserver::dataholders
