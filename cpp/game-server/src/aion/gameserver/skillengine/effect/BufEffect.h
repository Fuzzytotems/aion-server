#pragma once

#include "aion/gameserver/skillengine/effect/BufEffect.xml.h"

#include <vector>

#include "aion/gameserver/model/stats/calc/functions/fwd.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.BufEffect. @author ATracer */
class BufEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/BufEffect.xml.inc"
public:
	void applyEffect(model::Effect& effect) const override;

	/** Will be called from effect controller when effect starts */
	void startEffect(model::Effect& effect) const override;

	using EffectTemplate::getModifiers; // the generated getter of the action modifiers (C++ name hiding)

protected:
	/** @return the new stat functions of the effect's changes (Java List<IStatFunction>: this list holds the only references) */
	virtual std::vector<runtime::Ref<gameserver::model::stats::calc::functions::IStatFunction>> getModifiers(model::Effect& effect) const;
};

} // namespace aion::gameserver::skillengine::effect
