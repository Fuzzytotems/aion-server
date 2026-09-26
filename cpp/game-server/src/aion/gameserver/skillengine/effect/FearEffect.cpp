#include "aion/gameserver/skillengine/effect/FearEffect.h"

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
#include "aion/gameserver/controllers/observer/ActionObserver.h"
#include "aion/gameserver/controllers/observer/ObserverType.h"
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
 * Java: the anonymous ActionObserver(ObserverType.ATTACKED) of FearEffect.startEffect (FearEffect.java:80-87, fieldmap key FearEffect$1), added
 * only for a template whose resistchance is below 100 (a hit may then break the fear, e.g. 540 Terrible Howl). Stored in the effected creature's
 * ObserveController and, through the removal task of Effect.addObserver, in the effect's observerRemoveTasks; Effect.endEffect -> removeObservers
 * removes it from both (cycles.toml "FearEffect$1#effect" and "FearEffect$1#effected": java-hook). The template is immutable static data
 * (fieldmap: captured this: final template reference).
 * <p>
 * Java's body reads the template's protected resistchance through the captured this. FearEffect.h has no friend line for this struct (the
 * pre-approval of M5b-2 part 3 covered RootEffect, HideEffect and ProvokerEffect only), so startEffect, which may read the field, hands its value
 * to the constructor: the same value at the same moment, because the template never changes after load (docs/deviations/P5-03.md, header
 * request `friend struct FearEffect_ActionObserver;`).
 */
struct FearEffect_ActionObserver final : controllers::observer::ActionObserver {
	AION_MAKE_REF_FRIEND

	const FearEffect* fearEffect; // captured this FearEffect this (line 84)
	const Ref<model::Effect> effect; // captured param Effect effect (line 85)
	const Ref<Creature> effected; // captured local Creature effected (line 85)
	const int32_t resistchance; // fieldmap: C++ only, fearEffect->resistchance (protected, no friend line), immutable static data

	static Ref<FearEffect_ActionObserver> create(const FearEffect& fearEffect, int32_t resistchance, model::Effect& effect, Creature& effected) {
		return runtime::makeRef<FearEffect_ActionObserver>(fearEffect, resistchance, effect, effected);
	}

	void attacked(Creature& /*creature*/, int32_t /*skillId*/) override {
		if (commons::utils::Rnd::chance() >= static_cast<float>(resistchance))
			effected->getEffectController()->removeEffect(effect->getSkillId());
	}

protected:
	FearEffect_ActionObserver(const FearEffect& fearEffectValue, int32_t resistchanceValue, model::Effect& effectValue, Creature& effectedValue)
		: ActionObserver(controllers::observer::ObserverType::ATTACKED), fearEffect(&fearEffectValue), effect(Ref<model::Effect>(effectValue)),
		  effected(Ref<Creature>(effectedValue)), resistchance(resistchanceValue) {}
	~FearEffect_ActionObserver() override = default;
};

/**
 * Java: the nested record FearEffect.FearTask (FearEffect.java:102-122), scheduled at a fixed rate of 1 s while the fear lasts and
 * GeoDataConfig.FEAR_ENABLE is on. FearEffect.h declares no nested FearTask (the census counts `run` as an undeclared body), so the record is this
 * file-local task struct, mapped to its Java name for the concurrency lint (docs/deviations/P5-03.md, header request). A task object with only
 * TaskArg members, ported as an aggregate TaskStruct value (runtime-architecture.md §7.3, the WalkManager$1 precedent): the two Refs retain the
 * creatures exactly as the record's final fields do. Effect.setPeriodicTask keeps the Future, and Effect.endEffect -> stopTasks cancels it,
 * which releases the task and its Refs (cycles.toml "FearEffect.FearTask.effector" / ".effected": java-hook). The record's equals/hashCode have no
 * caller.
 */
// fieldmap-class: com.aionemu.gameserver.skillengine.effect.FearEffect.FearTask
struct FearEffect_FearTask final : runtime::TaskStruct {
	// The flee point is farther than the target travels per tick (1 sec):
	// the target never reaches it between ticks and keeps running smoothly - same as retail behavior.
	static constexpr float FLEE_SECONDS = 2.5f;

	const Ref<Creature> effector;
	const Ref<Creature> effected;

	void operator()() const {
		if (effected->getEffectController()->isUnderFear() && utils::PositionUtil::isInRange(*effected, *effector, 40)) {
			float angle = utils::PositionUtil::calculateAngleFrom(*effector, *effected);
			float maxDistance = effected->getGameStats()->getMovementSpeedFloat() * FLEE_SECONDS;
			geoEngine::math::Vector3f closestCollision = world::geo::GeoService::getInstance().findMovementCollision(*effected, angle, maxDistance);
			if (Ptr<Npc> npc = runtime::as<Npc>(effected)) {
				npc->getMoveController()->moveToPoint(closestCollision.getX(), closestCollision.getY(), closestCollision.getZ());
			} else {
				int8_t moveAwayHeading = utils::PositionUtil::convertAngleToHeading(angle);
				effected->getMoveController()->setNewDirection(closestCollision.getX(), closestCollision.getY(), closestCollision.getZ(),
					moveAwayHeading);
				effected->getMoveController()->startMovingToDestination();
			}
		}
	}
};

void FearEffect::applyEffect(model::Effect& effect) const {
	Ptr<Creature> effected = effect.getEffected();
	effected->getEffectController()->removeHideEffects();
	// Fear stops gliding
	if (Ptr<Player> player = runtime::as<Player>(effected); player && player->isInGlidingState()) {
		player->getFlyController().onStopGliding();
	}
	effect.addToEffectedController();
}

void FearEffect::calculate(model::Effect& effect) const {
	EffectTemplate::calculate(effect, gameserver::model::stats::container::StatEnum::FEAR_RESISTANCE, std::nullopt);
}

// Anonymous class com.aionemu.gameserver.skillengine.effect.FearEffect$1 and record FearEffect.FearTask: the structs above
void FearEffect::startEffect(model::Effect& effect) const {
	const Ptr<Creature> effector = effect.isReflected() ? effect.getOriginalEffected() : effect.getEffector();
	const Ptr<Creature> effected = effect.getEffected();
	effected->getController().cancelCurrentSkill(effector);
	effect.setAbnormal(AbnormalState::FEAR);
	effected->getEffectController()->setAbnormal(AbnormalState::FEAR);
	effected->getMoveController()->abortMove();
	if (Ptr<Npc> npc = runtime::as<Npc>(effected)) {
		ai::manager::EmoteManager::emoteStartAttacking(*npc, *effector); // set weapon_equipped for faster walk speed
		effected->getAi().setStateIfNot(ai::AIState::FEAR);
	} else if (runtime::as<Player>(effected) && effected->isInState(CreatureState::WALK_MODE)) {
		effected->unsetState(CreatureState::WALK_MODE);
		utils::PacketSendUtility::broadcastPacket(*runtime::cast<Player>(effected),
			network::aion::serverpackets::SM_EMOTION(*effected, gameserver::model::EmotionType::RUN), true);
	}
	if (configs::main::GeoDataConfig::FEAR_ENABLE.load()) {
		runtime::FutureRef fearTask =
			utils::ThreadPoolManager::getInstance().scheduleAtFixedRate(FearEffect_FearTask{{}, Ref<Creature>(*effector), Ref<Creature>(*effected)}, 0, 1000);
		effect.setPeriodicTask(fearTask, position);
	}
	// resistchance of fear effect to damage, if value is lower than 100, fear can be interrupted bz damage
	// example skillId: 540 Terrible howl
	if (resistchance < 100) {
		effect.addObserver(*effected, *FearEffect_ActionObserver::create(*this, resistchance, effect, *effected));
	}
}

void FearEffect::endEffect(model::Effect& effect) const {
	effect.getEffected()->getEffectController()->unsetAbnormal(AbnormalState::FEAR);
	effect.getEffected()->getMoveController()->abortMove();
	utils::PacketSendUtility::broadcastPacketAndReceive(*effect.getEffected(), network::aion::serverpackets::SM_POSITION(*effect.getEffected()));
	if (runtime::as<Npc>(effect.getEffected())) {
		effect.getEffected()->getAi().setStateIfNot(ai::AIState::IDLE);
		effect.getEffected()->getAi().onCreatureEvent(ai::event::AIEventType::ATTACK, *effect.getEffected());
	}
}

} // namespace aion::gameserver::skillengine::effect
