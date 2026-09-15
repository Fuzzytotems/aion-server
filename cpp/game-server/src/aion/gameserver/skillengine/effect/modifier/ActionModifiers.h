#pragma once

#include "aion/gameserver/skillengine/effect/modifier/ActionModifiers.xml.h"

#include <memory>
#include <vector>

namespace aion::gameserver::skillengine::effect::modifier {

/**
 * Java com.aionemu.gameserver.skillengine.effect.modifier.ActionModifiers.
 * <p>
 * C++ notes (P4-08): `getActionModifiers()` returns the bound list; Java creates an empty list on first access when none was bound, which the
 * empty vector stands for.
 *
 * @author ATracer
 */
class ActionModifiers : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/skillengine/effect/modifier/ActionModifiers.xml.inc"
public:
	const std::vector<std::unique_ptr<ActionModifier>>& getActionModifiers() const { return actionModifiers; }
};

} // namespace aion::gameserver::skillengine::effect::modifier
