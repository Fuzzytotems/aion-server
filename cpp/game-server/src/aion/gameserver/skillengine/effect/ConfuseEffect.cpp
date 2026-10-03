#include "aion/gameserver/skillengine/effect/ConfuseEffect.h"

#include <cstdint>
#include <optional>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/ai/event/AIEventType.h"
#include "aion/gameserver/ai/manager/EmoteManager.h"
#include "aion/gameserver/configs/main/GeoDataConfig.h"
#include "aion/gameserver/controllers/CreatureController.h"
#include "aion/gameserver/controllers/FlyController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/controllers/movement/CreatureMoveController.h"
#include "aion/gameserver/controllers/movement/NpcMoveController.h"
#include "aion/gameserver/geoEngine/math/Vector3f.h"
#include "aion/gameserver/model/EmotionType.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/state/CreatureState.h"
#include "aion/gameserver/model/stats/container/CreatureGameStats.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_POSITION.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/sched/Future.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/world/geo/GeoService.h"

namespace aion::gameserver::skillengine::effect {

using gameserver::model::gameobjects::Creature;
using gameserver::model::gameobjects::Npc;
using gameserver::model::gameobjects::player::Player;
using gameserver::model::gameobjects::state::CreatureState;
using runtime::Ptr;
using runtime::Ref;

/**
 * Java: the nested record ConfuseEffect.ConfuseTask (ConfuseEffect.java:84-103), scheduled at a fixed rate of 1 s while the confusion lasts and
 * GeoDataConfig.FEAR_ENABLE is on. ConfuseEffect.h declares no nested ConfuseTask, so the record is this file-local task struct, mapped to its
 * Java name for the concurrency lint, as FearEffect's FearTask is (docs/deviations/P5-03.md). A task object with only TaskArg members, an
 * aggregate TaskStruct value: the Ref retains the creature exactly as the record's final field does. Effect.setPeriodicTask keeps the Future, and
 * Effect.endEffect -> stopTasks cancels it, which releases the task and its Ref. The record's equals/hashCode have no caller.
 */
// fieldmap-class: com.aionemu.gameserver.skillengine.effect.ConfuseEffect.ConfuseTask
struct ConfuseEffect_ConfuseTask final : runtime::TaskStruct {
	// The flee point is farther than the target travels per tick (1 sec):
	// the target never reaches it between ticks and keeps running smoothly - same as retail behavior.
	static constexpr float FLEE_SECONDS = 2.5f;

	const Ref<Creature> effected;

	void operator()() const {
		if (effected->getEffectController()->isConfused()) {
			float angle = commons::utils::Rnd::nextFloat(360.0f);
			float maxDistance = effected->getGameStats()->getMovementSpeedFloat() * FLEE_SECONDS;
			geoEngine::math::Vector3f closestCollision = world::geo::GeoService::getInstance().findMovementCollision(*effected, angle, maxDistance);
			if (Ptr<Npc> npc = runtime::as<Npc>(effected)) {
				npc->getMoveController()->moveToPoint(closestCollision.getX(), closestCollision.getY(), closestCollision.getZ());
			} else {
				int8_t heading = utils::PositionUtil::convertAngleToHeading(angle);
				effected->getMoveController()->setNewDirection(closestCollision.getX(), closestCollision.getY(), closestCollision.getZ(), heading);
				effected->getMoveController()->startMovingToDestination();
			}
		}
	}
};

// Java ConfuseEffect.java:37-45
void ConfuseEffect::applyEffect(model::Effect& effect) const {
	Ptr<Creature> effected = effect.getEffected();
	effected->getEffectController()->removeHideEffects();
	if (Ptr<Player> player = runtime::as<Player>(effected); player && player->isInGlidingState()) {
		player->getFlyController().onStopGliding();
	}
	effect.addToEffectedController();
}

// Java ConfuseEffect.java:48-50
void ConfuseEffect::calculate(model::Effect& effect) const {
	EffectTemplate::calculate(effect, gameserver::model::stats::container::StatEnum::CONFUSE_RESISTANCE, std::nullopt);
}

// Java ConfuseEffect.java:53-71; record ConfuseEffect.ConfuseTask: the struct above
void ConfuseEffect::startEffect(model::Effect& effect) const {
	const Ptr<Creature> effector = effect.isReflected() ? effect.getOriginalEffected() : effect.getEffector();
	const Ptr<Creature> effected = effect.getEffected();
	effected->getController().cancelCurrentSkill(effect.getEffector());
	effected->getEffectController()->setAbnormal(AbnormalState::CONFUSE);
	effect.setAbnormal(AbnormalState::CONFUSE);
	effected->getMoveController()->abortMove();
	if (Ptr<Npc> npc = runtime::as<Npc>(effected)) {
		ai::manager::EmoteManager::emoteStartAttacking(*npc, *effector);
		effected->getAi().setStateIfNot(ai::AIState::CONFUSE);
	} else if (runtime::as<Player>(effected) && effected->isInState(CreatureState::WALK_MODE)) {
		effected->unsetState(CreatureState::WALK_MODE);
		utils::PacketSendUtility::broadcastPacket(*runtime::cast<Player>(effected),
			network::aion::serverpackets::SM_EMOTION(*effected, gameserver::model::EmotionType::RUN), true);
	}
	if (configs::main::GeoDataConfig::FEAR_ENABLE.load()) {
		runtime::FutureRef confuseTask =
			utils::ThreadPoolManager::getInstance().scheduleAtFixedRate(ConfuseEffect_ConfuseTask{{}, Ref<Creature>(*effected)}, 0, 1000);
		effect.setPeriodicTask(confuseTask, position);
	}
}

// Java ConfuseEffect.java:74-82
void ConfuseEffect::endEffect(model::Effect& effect) const {
	effect.getEffected()->getEffectController()->unsetAbnormal(AbnormalState::CONFUSE);
	effect.getEffected()->getMoveController()->abortMove();
	utils::PacketSendUtility::broadcastPacketAndReceive(*effect.getEffected(), network::aion::serverpackets::SM_POSITION(*effect.getEffected()));
	if (runtime::as<Npc>(effect.getEffected())) {
		effect.getEffected()->getAi().setStateIfNot(ai::AIState::IDLE);
		effect.getEffected()->getAi().onCreatureEvent(ai::event::AIEventType::ATTACK, *effect.getEffected());
	}
}

} // namespace aion::gameserver::skillengine::effect
