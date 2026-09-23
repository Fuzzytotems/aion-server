#pragma once

#include "aion/gameserver/skillengine/effect/NoFlyEffect.xml.h"

#include <optional>

#include "aion/gameserver/model/stats/container/fwd.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.NoFlyEffect. @author Sippolo */
class NoFlyEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/NoFlyEffect.xml.inc"
public:
	using EffectTemplate::calculate; // C++ name hiding by the declaration below (hub-headers.md §9.1)

	void calculate(model::Effect& effect) const override;

	void applyEffect(model::Effect& effect) const override;

protected:
	bool isDodgedOrResisted(model::Effect& effect, std::optional<gameserver::model::stats::container::StatEnum> statEnum) const override;

public:
	void startEffect(model::Effect& effect) const override;

	void endEffect(model::Effect& effect) const override;
};

} // namespace aion::gameserver::skillengine::effect
