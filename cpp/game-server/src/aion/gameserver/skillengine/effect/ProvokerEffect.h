#pragma once

#include "aion/gameserver/skillengine/effect/ProvokerEffect.xml.h"

#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.ProvokerEffect. @author ATracer, kecimis */
class ProvokerEffect : public ::aion::gameserver::skillengine::effect::ShieldEffect {
#include "aion/gameserver/skillengine/effect/ProvokerEffect.xml.inc"
	friend struct ProvokerEffect_ActionObserver;
public:
	void applyEffect(model::Effect& effect) const override;

	void startEffect(model::Effect& effect) const override;

private:
	bool shouldApply(gameserver::model::gameobjects::Creature& effector, gameserver::model::gameobjects::Creature& target, int32_t attackSkillId) const;

	runtime::Ptr<gameserver::model::gameobjects::Creature> getProvokeTarget(gameserver::model::gameobjects::Creature& effector,
		gameserver::model::gameobjects::Creature& target) const;

public:
	void endEffect(model::Effect& effect) const override;
};

} // namespace aion::gameserver::skillengine::effect
