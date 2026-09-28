#pragma once

#include "aion/gameserver/skillengine/effect/AbstractAbsoluteStatEffect.xml.h"

#include <vector>

#include "aion/gameserver/model/stats/calc/functions/fwd.h"
#include "aion/gameserver/model/templates/stats/fwd.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.AbstractAbsoluteStatEffect. @author Rolandas */
class AbstractAbsoluteStatEffect : public ::aion::gameserver::skillengine::effect::BufEffect {
#include "aion/gameserver/skillengine/effect/AbstractAbsoluteStatEffect.xml.inc"
public:
	using BufEffect::getModifiers; // the generated getter of the action modifiers (C++ name hiding)

protected:
	std::vector<runtime::Ref<gameserver::model::stats::calc::functions::IStatFunction>> getModifiers(model::Effect& effect) const override;

public:
	/** @return the absolute stat set of statSetId (Java null: nullptr) */
	const gameserver::model::templates::stats::ModifiersTemplate* getModifiersSet() const;
};

} // namespace aion::gameserver::skillengine::effect
