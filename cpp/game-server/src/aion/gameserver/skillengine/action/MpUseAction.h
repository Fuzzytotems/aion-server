#pragma once

#include "aion/gameserver/skillengine/action/MpUseAction.xml.h"

namespace aion::gameserver::skillengine::action {

/** Java com.aionemu.gameserver.skillengine.action.MpUseAction. @author ATracer */
class MpUseAction : public ::aion::gameserver::skillengine::action::Action {
#include "aion/gameserver/skillengine/action/MpUseAction.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::action
