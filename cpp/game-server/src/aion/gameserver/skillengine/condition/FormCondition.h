#pragma once

#include "aion/gameserver/skillengine/condition/FormCondition.xml.h"

namespace aion::gameserver::skillengine::condition {

/** Java com.aionemu.gameserver.skillengine.condition.FormCondition. @author kecimis */
class FormCondition : public ::aion::gameserver::skillengine::condition::Condition {
#include "aion/gameserver/skillengine/condition/FormCondition.xml.inc"
public:
};

} // namespace aion::gameserver::skillengine::condition
