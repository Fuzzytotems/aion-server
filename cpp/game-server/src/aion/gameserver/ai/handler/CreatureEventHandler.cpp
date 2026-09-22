#include "aion/gameserver/ai/handler/CreatureEventHandler.h"

#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/ai/AISubState.h"
#include "aion/gameserver/ai/NpcAI.h"
#include "aion/gameserver/ai/event/AIEventType.h"
#include "aion/gameserver/ai/handler/ShoutEventHandler.h"
#include "aion/gameserver/ai/manager/AttackManager.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/state/CreatureVisualState.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/npc/NpcTemplateType.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/TribeRelationService.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/geo/GeoService.h"

namespace aion::gameserver::ai::handler {

using event::AIEventType;
using manager::AttackManager;
using model::gameobjects::Creature;
using model::gameobjects::Npc;
using model::gameobjects::VisibleObject;
using model::gameobjects::player::Player;
using model::gameobjects::state::CreatureVisualState;
using model::templates::npc::NpcTemplateType;
using services::TribeRelationService;
using skillengine::effect::AbnormalState;
using utils::PositionUtil;

namespace {

/** Java: creature.equals(npcAI.getTarget()) - AionObject.equals(null) is false (AionObject.java:57-58) */
bool javaEquals(Creature& creature, runtime::Ptr<VisibleObject> target) {
	return target && creature.equals(*target);
}

} // namespace

void CreatureEventHandler::onCreatureMoved(NpcAI& npcAI, Creature& creature) {
	checkAggro(npcAI, creature);
	if (runtime::Ptr<Player> player = runtime::as<Player>(creature)) {
		runtime::Ref<questEngine::model::QuestEnv> env =
			questEngine::model::QuestEnv::create(runtime::Ptr<VisibleObject>(npcAI.getOwner()), *player, 0);
		questEngine::QuestEngine::getInstance().onAtDistance(*env);
	}
}

void CreatureEventHandler::onCreatureSee(NpcAI& npcAI, Creature& creature) {
	if (npcAI.isInSubState(AISubState::TARGET_LOST) && javaEquals(creature, npcAI.getTarget())) { // see target again after hide end
		npcAI.setSubStateIfNot(AISubState::NONE);
		if (npcAI.isInState(AIState::FIGHT)) { // continue to attack
			AttackManager::scheduleNextAttack(npcAI);
			return;
		}
	}
	checkAggro(npcAI, creature);
	if (runtime::Ptr<Player> player = runtime::as<Player>(creature)) {
		runtime::Ref<questEngine::model::QuestEnv> env =
			questEngine::model::QuestEnv::create(runtime::Ptr<VisibleObject>(npcAI.getOwner()), *player, 0);
		questEngine::QuestEngine::getInstance().onAtDistance(*env);
	}
}

void CreatureEventHandler::checkAggro(NpcAI& ai, Creature& creature) {
	if (ai.isInState(AIState::FIGHT))
		return;

	if (ai.isInState(AIState::RETURNING))
		return;

	if (creature.isDead())
		return;

	if (creature.isInVisualState(CreatureVisualState::BLINKING))
		return;

	if (creature.isFlag())
		return;

	Npc& owner = ai.getOwner();
	if (!owner.isSpawned())
		return;

	if (!owner.canSee(runtime::Ptr<VisibleObject>(creature)))
		return;

	if (owner.getEffectController()->isAbnormalSet(AbnormalState::SANCTUARY))
		return;

	if (!owner.getPosition()->isMapRegionActive())
		return;

	if (isInSeeRange(creature, owner)) {
		ai.handleCreatureDetected(creature); // TODO: Move to AIEventType, prevent calling multiple times
		if (TribeRelationService::isAggressive(owner, creature) && !TribeRelationService::isFriend(owner, creature) &&
			creature.isEnemyFrom(owner)) {
			if (validateAggro(owner, creature) && world::geo::GeoService::getInstance().canSee(owner, creature)) {
				ShoutEventHandler::onSee(ai, creature);
				if (ai.canThink())
					ai.onCreatureEvent(AIEventType::CREATURE_AGGRO, creature);
			}
		} else { // non aggressive (should we consider also using geo canSee checks here?)
			ShoutEventHandler::onSee(ai, creature);
		}
	}
}

bool CreatureEventHandler::isInSeeRange(Creature& creature, Npc& npc) {
	if (npc.getAggroAngle() == 0 || npc.getAggroRange() == 0)
		return false;
	if (!PositionUtil::isInRange(npc, creature, static_cast<float>(npc.getAggroRange()), false))
		return false;
	return PositionUtil::isInFrontOf(creature, npc, npc.getAggroAngle() / 2.0f) ||
		PositionUtil::isInRange(npc, creature, static_cast<float>(npc.getShortAggroRange()), false);
}

bool CreatureEventHandler::validateAggro(Npc& owner, Creature& creature) {
	return creature.getLevel() - owner.getLevel() < 10 || owner.getObjectTemplate()->getNpcTemplateType() == NpcTemplateType::GUARD ||
		owner.getObjectTemplate()->getNpcTemplateType() == NpcTemplateType::ABYSS_GUARD;
}

} // namespace aion::gameserver::ai::handler
