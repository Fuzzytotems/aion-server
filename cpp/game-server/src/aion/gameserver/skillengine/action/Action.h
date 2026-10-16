#pragma once

#include "aion/gameserver/skillengine/action/Action.xml.h"

#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::action {

/** Java com.aionemu.gameserver.skillengine.action.Action. @author ATracer */
class Action : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/skillengine/action/Action.xml.inc"
public:
	/** Perform action specified in template */
	virtual bool act(model::Skill& skill) const = 0;

	/**
	 * Checks whether act(Skill) could be performed, without performing it.
	 *
	 * @return True, if the action can be performed
	 */
	virtual bool canAct(model::Skill& skill) const;
};

} // namespace aion::gameserver::skillengine::action
