#pragma once

#include "aion/gameserver/skillengine/effect/FallEffect.xml.h"

#include <optional>

#include "aion/gameserver/model/stats/container/fwd.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.FallEffect. @author Sippolo */
class FallEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/FallEffect.xml.inc"
protected:
	bool isDodgedOrResisted(model::Effect& effect, std::optional<gameserver::model::stats::container::StatEnum> statEnum) const override;

public:
	void applyEffect(model::Effect& effect) const override;
};

} // namespace aion::gameserver::skillengine::effect
