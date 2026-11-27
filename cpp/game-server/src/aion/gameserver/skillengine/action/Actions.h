#pragma once

#include "aion/gameserver/skillengine/action/Actions.xml.h"

#include <memory>
#include <vector>

namespace aion::gameserver::skillengine::action {

/**
 * Java com.aionemu.gameserver.skillengine.action.Actions.
 * <p>
 * C++ notes (P4-08): `getActions()` returns the bound list, empty when none was bound (Java: List.of(), upstream 15eaf77b6).
 *
 * @author ATracer
 */
class Actions : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/skillengine/action/Actions.xml.inc"
public:
	const std::vector<std::unique_ptr<Action>>& getActions() const { return actions; }
};

} // namespace aion::gameserver::skillengine::action
