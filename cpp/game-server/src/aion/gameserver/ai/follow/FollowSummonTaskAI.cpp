#include "aion/gameserver/ai/follow/FollowSummonTaskAI.h"

#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/ai/event/AIEventType.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Summon.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/summons/UnsummonType.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/summons/SummonsService.h"
#include "aion/gameserver/utils/PositionUtil.h"

namespace aion::gameserver::ai::follow {

using model::gameobjects::Creature;
using model::gameobjects::Summon;
using model::gameobjects::player::Player;
using utils::PositionUtil;

FollowSummonTaskAI::FollowSummonTaskAI(Creature& targetValue, Summon& summonValue)
	: target(targetValue), summon(summonValue),
	  // Java: this.master = summon.getMaster() - a Summon's master is a Player (Summon.h: the covariant override keeps Ptr<Creature>). The
	  // fieldmap makes the member a non-null Ref, so a summon without a master throws NullPointerException here instead of in the first
	  // isInMasterRange(); a summon always has one (SummonsService creates it with its master).
	  master(*runtime::cast<Player>(summonValue.getMaster())) {
	setLeadingCoordinates();
}

FollowSummonTaskAI::~FollowSummonTaskAI() = default;

runtime::Ref<FollowSummonTaskAI> FollowSummonTaskAI::create(Creature& target, Summon& summon) {
	return runtime::makeRef<FollowSummonTaskAI>(target, summon);
}

void FollowSummonTaskAI::setLeadingCoordinates() {
	targetX.set(target->getX());
	targetY.set(target->getY());
	targetZ.set(target->getZ());
}

void FollowSummonTaskAI::run() {
	if (!isInMasterRange()) {
		services::summons::SummonsService::release(*summon, model::summons::UnsummonType::DISTANCE);
		return;
	}
	if (!isInTargetRange()) {
		if (targetX.get() != target->getX() || targetY.get() != target->getY() || targetZ.get() != target->getZ()) {
			setLeadingCoordinates();
			onOutOfTargetRange();
		}
	} else if (!master->equals(*target)) {
		onDestination();
	}
}

bool FollowSummonTaskAI::isInTargetRange() {
	return PositionUtil::isInRange(*target, *summon, 2, false);
}

bool FollowSummonTaskAI::isInMasterRange() {
	return PositionUtil::isInRange(*master, *summon, 50);
}

void FollowSummonTaskAI::onDestination() {
	summon->getAi().onCreatureEvent(event::AIEventType::ATTACK, *target);
}

void FollowSummonTaskAI::onOutOfTargetRange() {
	summon->getAi().onGeneralEvent(event::AIEventType::MOVE_VALIDATE);
}

} // namespace aion::gameserver::ai::follow
