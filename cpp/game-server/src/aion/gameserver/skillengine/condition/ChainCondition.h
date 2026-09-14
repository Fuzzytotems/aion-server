#pragma once

#include "aion/gameserver/skillengine/condition/ChainCondition.xml.h"

namespace aion::gameserver::skillengine::condition {

/** Java com.aionemu.gameserver.skillengine.condition.ChainCondition. @author ATracer, kecimis, Neon */
class ChainCondition : public ::aion::gameserver::skillengine::condition::Condition {
#include "aion/gameserver/skillengine/condition/ChainCondition.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::condition
