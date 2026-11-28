#pragma once

#include "aion/gameserver/skillengine/effect/modifier/ActionModifiers.xml.h"

#include <memory>
#include <vector>

namespace aion::gameserver::skillengine::effect::modifier {

/**
 * Java com.aionemu.gameserver.skillengine.effect.modifier.ActionModifiers.
 * <p>
 * C++ notes (P4-08): `getActionModifiers()` returns the bound list, empty when none was bound (Java: List.of(), upstream 15eaf77b6).
 *
 * @author ATracer
 */
class ActionModifiers : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/skillengine/effect/modifier/ActionModifiers.xml.inc"
public:
	const std::vector<std::unique_ptr<ActionModifier>>& getActionModifiers() const { return actionModifiers; }
};

} // namespace aion::gameserver::skillengine::effect::modifier
