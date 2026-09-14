#pragma once

#include "aion/gameserver/skillengine/action/Actions.xml.h"

namespace aion::gameserver::skillengine::action {

/** Java com.aionemu.gameserver.skillengine.action.Actions. @author ATracer */
class Actions : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/skillengine/action/Actions.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::action
