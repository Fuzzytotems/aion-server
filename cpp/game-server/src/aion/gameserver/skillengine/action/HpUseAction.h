#pragma once

#include "aion/gameserver/skillengine/action/HpUseAction.xml.h"

#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::action {

/** Java com.aionemu.gameserver.skillengine.action.HpUseAction. @author ATracer */
class HpUseAction : public ::aion::gameserver::skillengine::action::Action {
#include "aion/gameserver/skillengine/action/HpUseAction.xml.inc"
public:
	bool act(model::Skill& skill) const override;

	bool canAct(model::Skill& skill) const override;

private:
	// header-request: m5b2-p2-3 (Java private HpUseAction.getCost, HpUseAction.java; additive, non-virtual)
	int32_t getCost(model::Skill& skill) const;
};

} // namespace aion::gameserver::skillengine::action
