#include "aion/gameserver/skillengine/effect/SearchEffect.h"

#include <optional>

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PLAYER_STATE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::skillengine::effect {

using gameserver::model::gameobjects::Creature;
using gameserver::model::gameobjects::state::CreatureSeeState;

namespace {

/** Java Creature.setSeeState / unsetSeeState read seeState.getId(): a template without state= is a NullPointerException there */
CreatureSeeState unboxed(const std::optional<CreatureSeeState>& state) {
	if (!state)
		throw runtime::NullPointerException("SearchEffect.state is null");
	return *state;
}

} // namespace

void SearchEffect::applyEffect(model::Effect& effect) const {
	effect.addToEffectedController();
}

void SearchEffect::endEffect(model::Effect& effect) const {
	runtime::Ptr<Creature> effected = effect.getEffected();
	effected->unsetSeeState(unboxed(state));
	effected->updateKnownlist();
	utils::PacketSendUtility::broadcastPacketAndReceive(*effected, network::aion::serverpackets::SM_PLAYER_STATE(*effected));
}

void SearchEffect::startEffect(model::Effect& effect) const {
	runtime::Ptr<Creature> effected = effect.getEffected();
	effected->setSeeState(unboxed(state));
	effected->updateKnownlist();
	utils::PacketSendUtility::broadcastPacketAndReceive(*effected, network::aion::serverpackets::SM_PLAYER_STATE(*effected));
}

} // namespace aion::gameserver::skillengine::effect
