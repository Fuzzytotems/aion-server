#pragma once

#include "aion/gameserver/skillengine/properties/Properties.xml.h"

namespace aion::gameserver::skillengine::properties {

/** Java com.aionemu.gameserver.skillengine.properties.Properties. @author ATracer */
class Properties : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/skillengine/properties/Properties.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::properties
