#pragma once

#include "aion/gameserver/skillengine/action/ItemUseAction.xml.h"

namespace aion::gameserver::skillengine::action {

/** Java com.aionemu.gameserver.skillengine.action.ItemUseAction. @author ATracer */
class ItemUseAction : public ::aion::gameserver::skillengine::action::Action {
#include "aion/gameserver/skillengine/action/ItemUseAction.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::action
