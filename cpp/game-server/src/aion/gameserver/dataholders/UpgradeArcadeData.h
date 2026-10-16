#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/dataholders/UpgradeArcadeData.xml.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.UpgradeArcadeData.
 * <p>
 * C++: the level getters throw NullPointerException without a <levels> element, as Java does.
 *
 * @author ginho1
 */
class UpgradeArcadeData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/UpgradeArcadeData.xml.inc"
public:
	int32_t size() const;

	int32_t getMinResumableLevel() const;

	const std::vector<model::templates::event::upgradearcade::ArcadeLevel>& getUpgradeLevels() const;

	const model::templates::event::upgradearcade::ArcadeLevel* getMaxUpgradeLevel() const;
};

} // namespace aion::gameserver::dataholders
