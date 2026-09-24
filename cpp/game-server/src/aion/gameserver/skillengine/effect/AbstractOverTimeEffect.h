#pragma once

#include "aion/gameserver/skillengine/effect/AbstractOverTimeEffect.xml.h"

#include <optional>

#include "aion/gameserver/skillengine/effect/fwd.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.AbstractOverTimeEffect. @author kecimis */
class AbstractOverTimeEffect : public ::aion::gameserver::skillengine::effect::EffectTemplate {
#include "aion/gameserver/skillengine/effect/AbstractOverTimeEffect.xml.inc"
public:
	void applyEffect(model::Effect& effect) const override;

	int32_t getValue() const override { return value; }

	/** on retail these effects last one sec more than their template value of duration2 (Java int arithmetic: wraps above 2147482647) */
	int32_t getDuration2() const override { return static_cast<int32_t>(static_cast<uint32_t>(duration2) + 1000u); }

	void startEffect(model::Effect& effect) const override;

	/** Java startEffect(Effect, AbnormalState): `abnormal` is null (std::nullopt) for effects without an abnormal state */
	void startEffect(model::Effect& effect, std::optional<AbnormalState> abnormal) const;

	using EffectTemplate::endEffect; // C++ name hiding by the overload below

	void endEffect(model::Effect& effect, std::optional<AbnormalState> abnormal) const;
};

} // namespace aion::gameserver::skillengine::effect
