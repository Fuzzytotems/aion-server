#include "aion/gameserver/skillengine/effect/RideRobotEffect.h"

#include <vector>

#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/controllers/effect/PlayerEffectController.h"
#include "aion/gameserver/controllers/observer/ActionObserver.h"
#include "aion/gameserver/controllers/observer/ObserverType.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/enums/EquipType.h"
#include "aion/gameserver/network/aion/serverpackets/SM_RIDE_ROBOT.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::skillengine::effect {

using gameserver::model::gameobjects::Item;
using gameserver::model::gameobjects::player::Player;
using runtime::Ptr;
using runtime::Ref;

/**
 * Java: the anonymous ActionObserver(ObserverType.UNEQUIP) of RideRobotEffect.startEffect (RideRobotEffect.java:28-35, fieldmap key
 * RideRobotEffect$1): taking off a weapon ends the ride. It reads nothing of the template. Stored in the rider's ObserveController and, through
 * the removal task of Effect.addObserver, in the effect's observerRemoveTasks; Effect.endEffect -> removeObservers removes it from both
 * (cycles.toml "RideRobotEffect$1#effect": java-hook).
 */
struct RideRobotEffect_ActionObserver final : controllers::observer::ActionObserver {
	AION_MAKE_REF_FRIEND

	const Ref<model::Effect> effect; // captured param Effect effect (line 33)

	static Ref<RideRobotEffect_ActionObserver> create(model::Effect& effect) { return runtime::makeRef<RideRobotEffect_ActionObserver>(effect); }

	void unequip(Item& item, Player& /*owner*/) override {
		if (item.getEquipmentType() == gameserver::model::templates::item::enums::EquipType::WEAPON) {
			effect->endEffect();
		}
	}

protected:
	explicit RideRobotEffect_ActionObserver(model::Effect& effectValue)
		: ActionObserver(controllers::observer::ObserverType::UNEQUIP), effect(Ref<model::Effect>(effectValue)) {}
	~RideRobotEffect_ActionObserver() override = default;
};

void RideRobotEffect::applyEffect(model::Effect& effect) const {
	effect.addToEffectedController();
}

// Anonymous class com.aionemu.gameserver.skillengine.effect.RideRobotEffect$1: the callback struct above
void RideRobotEffect::startEffect(model::Effect& effect) const {
	const Ptr<Player> player = runtime::cast<Player>(effect.getEffected());
	player->setRobotId(player->getEquipment().getMainHandWeapon()->getItemSkinTemplate()->getRobotId());
	utils::PacketSendUtility::broadcastPacketAndReceive(*player, network::aion::serverpackets::SM_RIDE_ROBOT(*player));
	effect.addObserver(*player, *RideRobotEffect_ActionObserver::create(effect));
}

void RideRobotEffect::endEffect(model::Effect& effect) const {
	const Ptr<Player> player = runtime::cast<Player>(effect.getEffected());
	player->setRobotId(0);
	utils::PacketSendUtility::broadcastPacketAndReceive(*player, network::aion::serverpackets::SM_RIDE_ROBOT(*player));
	for (const Ptr<model::Effect>& ef : player->getEffectController()->getAbnormalEffects()) {
		if (ef->getSkillTemplate()->getRideRobotCondition() != nullptr)
			ef->endEffect();
	}
}

} // namespace aion::gameserver::skillengine::effect
