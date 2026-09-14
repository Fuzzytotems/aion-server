#pragma once

#include "aion/gameserver/skillengine/action/Action.xml.h"

namespace aion::gameserver::skillengine::action {

/** Java com.aionemu.gameserver.skillengine.action.Action. @author ATracer */
class Action : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/skillengine/action/Action.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::action
