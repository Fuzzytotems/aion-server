#pragma once

#include "aion/gameserver/dataholders/GuideHtmlData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.GuideHtmlData. @author xTz */
class GuideHtmlData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/GuideHtmlData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
