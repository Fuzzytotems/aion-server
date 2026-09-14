#pragma once

#include "aion/gameserver/skillengine/action/HpUseAction.xml.h"

namespace aion::gameserver::skillengine::action {

/** Java com.aionemu.gameserver.skillengine.action.HpUseAction. @author ATracer */
class HpUseAction : public ::aion::gameserver::skillengine::action::Action {
#include "aion/gameserver/skillengine/action/HpUseAction.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::action
