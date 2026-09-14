#pragma once

#include "aion/gameserver/dataholders/BindPointData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.BindPointData. @author avol */
class BindPointData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/BindPointData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
