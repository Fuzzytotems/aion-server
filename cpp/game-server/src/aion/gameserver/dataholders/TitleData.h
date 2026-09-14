#pragma once

#include "aion/gameserver/dataholders/TitleData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.TitleData. @author xavier */
class TitleData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/TitleData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
