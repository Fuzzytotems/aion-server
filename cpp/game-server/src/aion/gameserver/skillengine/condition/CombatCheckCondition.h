#pragma once

#include "aion/gameserver/skillengine/condition/CombatCheckCondition.xml.h"

namespace aion::gameserver::skillengine::condition {

/** Java com.aionemu.gameserver.skillengine.condition.CombatCheckCondition. @author nrg */
class CombatCheckCondition : public ::aion::gameserver::skillengine::condition::Condition {
#include "aion/gameserver/skillengine/condition/CombatCheckCondition.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::condition
