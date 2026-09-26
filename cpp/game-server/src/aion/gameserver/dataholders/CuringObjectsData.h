#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/dataholders/CuringObjectsData.xml.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.CuringObjectsData.
 * <p>
 * C++: `curingObjects` points into the bound `curingObject` storage, which Java also keeps.
 *
 * @author xTz
 */
class CuringObjectsData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/CuringObjectsData.xml.inc"
private:
	std::vector<const model::templates::curingzones::CuringTemplate*> curingObjects;

public:
	int32_t size() const;

	const std::vector<const model::templates::curingzones::CuringTemplate*>& getCuringObject() const;
};

} // namespace aion::gameserver::dataholders
