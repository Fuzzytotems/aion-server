#include "aion/gameserver/skillengine/effect/TargetChangeEffect.h"

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/model/Effect.h"

namespace aion::gameserver::skillengine::effect {

using gameserver::model::gameobjects::Creature;
using gameserver::model::gameobjects::VisibleObject;
using gameserver::model::gameobjects::player::Player;
using runtime::Ptr;

void TargetChangeEffect::applyEffect(model::Effect& effect) const {
	const Ptr<Creature> effected = effect.getEffected();
	if (Ptr<Player> player = runtime::as<Player>(effected)) {
		Ptr<VisibleObject> target = nullptr;
		switch (delta) {
			// case 0: Shimmerbomb sets target to null
			case 1:
				target = effect.getEffector();
				break;
		}
		player->setTarget(target);
	}
}

} // namespace aion::gameserver::skillengine::effect
