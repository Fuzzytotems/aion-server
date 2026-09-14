#pragma once

#include "aion/gameserver/dataholders/CubeExpandData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.CubeExpandData. @author dragoon112 */
class CubeExpandData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/CubeExpandData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
