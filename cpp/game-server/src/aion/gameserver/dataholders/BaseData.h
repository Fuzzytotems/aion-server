#pragma once

#include <cstdint>

#include "aion/gameserver/dataholders/BaseData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.BaseData. @author Source, Estrayl */
class BaseData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/BaseData.xml.inc"
public:
	int32_t size() const;
};

} // namespace aion::gameserver::dataholders
