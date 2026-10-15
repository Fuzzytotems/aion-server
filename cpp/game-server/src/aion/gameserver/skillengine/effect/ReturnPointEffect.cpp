#include "aion/gameserver/skillengine/effect/ReturnPointEffect.h"

#include <string>

#include "aion/gameserver/model/EmotionType.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/state/CreatureState.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::skillengine::effect {

using gameserver::model::gameobjects::player::Player;
using gameserver::model::gameobjects::state::CreatureState;

// Java ReturnPointEffect.java:23-36
void ReturnPointEffect::applyEffect(model::Effect& effect) const {
	Player& player = *runtime::cast<Player>(effect.getEffector());
	if (player.isInState(CreatureState::RESTING)) {
		player.unsetState(CreatureState::RESTING);
		utils::PacketSendUtility::broadcastPacket(player,
			network::aion::serverpackets::SM_EMOTION(player, gameserver::model::EmotionType::STAND, 0, player.getX(), player.getY(), player.getZ(),
				player.getHeading(), getTargetObjectId(player)),
			true);
	}
	// Java: itemTemplate.getReturnWorldId() on the effect's item template - a NullPointerException for a skill cast without an item; calculate
	// adds no success effect then, so applyEffect is not reached that way
	const gameserver::model::templates::item::ItemTemplate* itemTemplate = effect.getItemTemplate();
	if (itemTemplate == nullptr)
		throw runtime::NullPointerException("Effect.getItemTemplate()");
	int32_t worldId = itemTemplate->getReturnWorldId();
	const std::string& pointAlias = itemTemplate->getReturnAlias();
	services::teleport::TeleportService::useTeleportScroll(*runtime::cast<Player>(effect.getEffector()), pointAlias, worldId);
}

// Java ReturnPointEffect.java:38-43
void ReturnPointEffect::calculate(model::Effect& effect) const {
	const gameserver::model::templates::item::ItemTemplate* itemTemplate = effect.getItemTemplate();
	if (itemTemplate != nullptr)
		effect.addSuccessEffect(this);
}

// Java ReturnPointEffect.java:45-47
int32_t ReturnPointEffect::getTargetObjectId(gameserver::model::gameobjects::player::Player& player) const {
	return player.getTarget() == nullptr ? 0 : player.getTarget()->getObjectId();
}

} // namespace aion::gameserver::skillengine::effect
