#pragma once

#include "aion/gameserver/skillengine/model/Motion.xml.h"

namespace aion::gameserver::skillengine::model {

/** Java com.aionemu.gameserver.skillengine.model.Motion. @author kecimis */
class Motion : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/skillengine/model/Motion.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::model
