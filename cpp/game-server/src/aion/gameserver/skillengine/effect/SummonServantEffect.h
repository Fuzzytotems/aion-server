#pragma once

#include "aion/gameserver/skillengine/effect/SummonServantEffect.xml.h"

#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::effect {

/** Java com.aionemu.gameserver.skillengine.effect.SummonServantEffect. @author ATracer */
class SummonServantEffect : public ::aion::gameserver::skillengine::effect::SummonEffect {
#include "aion/gameserver/skillengine/effect/SummonServantEffect.xml.inc"
public:
	void applyEffect(model::Effect& effect) const override;

protected:
	/**
	 * Called by SummonSkillAreaEffect and SummonTotemEffect too.
	 *
	 * @return the servant VisibleObjectSpawner.spawnServant just created (a new object: Ref, hub-headers.md §5)
	 */
	runtime::Ref<gameserver::model::gameobjects::Servant> spawnServant(model::Effect& effect, int32_t spawnDuration,
		gameserver::model::gameobjects::NpcObjectType npcObjectType, float x, float y, float z) const;
};

} // namespace aion::gameserver::skillengine::effect
