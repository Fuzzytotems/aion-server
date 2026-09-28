#pragma once

#include "aion/gameserver/skillengine/model/Times.xml.h"

namespace aion::gameserver::skillengine::model {

/** Java com.aionemu.gameserver.skillengine.model.Times. @author kecims */
class Times : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/skillengine/model/Times.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::model
