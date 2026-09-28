#include "aion/gameserver/skillengine/effect/SummonServantEffect.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::skillengine::effect {

void SummonServantEffect::applyEffect(model::Effect& /*effect*/) const {
	AION_UNPORTED();
}

runtime::Ref<gameserver::model::gameobjects::Servant> SummonServantEffect::spawnServant(model::Effect& /*effect*/, int32_t /*spawnDuration*/,
	gameserver::model::gameobjects::NpcObjectType /*npcObjectType*/, float /*x*/, float /*y*/, float /*z*/) const {
	AION_UNPORTED();
}

} // namespace aion::gameserver::skillengine::effect
