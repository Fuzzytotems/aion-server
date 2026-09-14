#pragma once

#include "aion/gameserver/dataholders/SignetDataTemplates.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.SignetDataTemplates. */
class SignetDataTemplates : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/SignetDataTemplates.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
