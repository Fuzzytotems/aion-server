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

private:
	// header-request: m5b2-p2-3 (Java private MpUseAction.getCost, MpUseAction.java; additive, non-virtual)
	int32_t getCost(model::Skill& skill) const;
};

} // namespace aion::gameserver::skillengine::action
