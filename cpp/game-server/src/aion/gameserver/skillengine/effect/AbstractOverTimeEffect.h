#pragma once

#include "aion/gameserver/skillengine/effect/AbstractOverTimeEffect.xml.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.AbstractOverTimeEffect. @author kecimis */
class AbstractOverTimeEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/AbstractOverTimeEffect.xml.inc"
public:
	void applyEffect(model::Effect& effect) const override;

	int32_t getValue() const override { return value; }

	/** on retail these effects last one sec more than their template value of duration2 */
	int32_t getDuration2() const override { return duration2 + 1000; }
};

} // namespace aion::gameserver::skillengine::effect
