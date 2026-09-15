#pragma once

#include "aion/gameserver/skillengine/action/MpUseAction.xml.h"

#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::action {

/** Java com.aionemu.gameserver.skillengine.action.MpUseAction. @author ATracer */
class MpUseAction : public ::aion::gameserver::skillengine::action::Action {
#include "aion/gameserver/skillengine/action/MpUseAction.xml.inc"
public:
	bool act(model::Skill& skill) const override;

	bool canAct(model::Skill& skill) const override;
};

} // namespace aion::gameserver::skillengine::action
