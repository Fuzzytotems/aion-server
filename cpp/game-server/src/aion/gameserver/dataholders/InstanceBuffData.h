#pragma once

#include <cstdint>
#include <unordered_map>

#include "aion/gameserver/dataholders/InstanceBuffData.xml.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.InstanceBuffData.
 * <p>
 * C++: the index points into the bound `instanceBonusattr` storage, which stays after afterUnmarshal (static-data.md §2.6).
 *
 * @author xTz
 */
class InstanceBuffData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/InstanceBuffData.xml.inc"
private:
	std::unordered_map<int32_t, const model::templates::instance_bonusatrr::InstanceBonusAttr*> templates;

public:
	int32_t size() const;

	/** @return the bonus attributes of the buff, nullptr (Java null) if there are none */
	const model::templates::instance_bonusatrr::InstanceBonusAttr* getInstanceBonusattr(int32_t buffId) const;
};

} // namespace aion::gameserver::dataholders
