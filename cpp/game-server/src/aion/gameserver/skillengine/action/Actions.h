#pragma once

#include "aion/gameserver/skillengine/action/Actions.xml.h"

#include <memory>
#include <vector>

namespace aion::gameserver::skillengine::action {

/**
 * Java com.aionemu.gameserver.skillengine.action.Actions.
 * <p>
 * C++ notes (P4-08): `getActions()` returns the bound list; Java creates an empty list on first access when none was bound, which the empty
 * vector stands for (a template is const after load, and no Java caller adds to the live list).
 *
 * @author ATracer
 */
class Actions : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/skillengine/action/Actions.xml.inc"
public:
	const std::vector<std::unique_ptr<Action>>& getActions() const { return actions; }
};

} // namespace aion::gameserver::skillengine::action
