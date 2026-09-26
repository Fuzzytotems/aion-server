#include "aion/gameserver/ai/handler/ShoutEventHandler.h"

#include <optional>
#include <string>
#include <vector>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/ai/NpcAI.h"
#include "aion/gameserver/ai/poll/AIQuestion.h"
#include "aion/gameserver/controllers/movement/NpcMoveController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/NpcShoutData.h"
#include "aion/gameserver/dataholders/WalkerData.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/npcshout/NpcShout.h"
#include "aion/gameserver/model/templates/npcshout/ShoutEventType.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/model/templates/walker/WalkerTemplate.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/NpcShoutsService.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::ai::handler {

namespace Rnd = commons::utils::Rnd;
using model::gameobjects::Creature;
using model::gameobjects::Npc;
using model::gameobjects::player::Player;
using model::templates::npcshout::NpcShout;
using model::templates::npcshout::ShoutEventType;
using services::NpcShoutsService;

namespace {

/**
 * Java passes the possibly-null List<NpcShout> of NpcShoutData.getNpcShouts straight into shoutRandom, whose first statement is
 * `if (shouts == null || shouts.isEmpty()) return;` (NpcShoutsService.java:60). The C++ signature takes a vector reference, so a null list
 * becomes the empty vector: shoutRandom cannot tell the two apart.
 */
const std::vector<const NpcShout*>& orEmpty(const std::optional<std::vector<const NpcShout*>>& shouts) {
	static const std::vector<const NpcShout*> none;
	return shouts ? *shouts : none;
}

} // namespace

void ShoutEventHandler::onSee(NpcAI& npcAI, Creature& target) {
	if (runtime::Ptr<Player> player = runtime::as<Player>(target); player && npcAI.ask(poll::AIQuestion::CAN_SHOUT)) {
		Npc& npc = npcAI.getOwner();
		std::optional<std::vector<const NpcShout*>> shouts =
			dataholders::DataManager::NPC_SHOUT_DATA->getNpcShouts(npc.getPosition()->getMapId(), npc.getNpcId(), ShoutEventType::SEE);
		NpcShoutsService::getInstance().shoutRandom(npc, player, orEmpty(shouts), 0);
	}
}

void ShoutEventHandler::onSpawn(NpcAI& npcAI) {
	if (npcAI.ask(poll::AIQuestion::CAN_SHOUT)) {
		Npc& npc = npcAI.getOwner();
		NpcShoutsService::getInstance().registerShoutTask(npc);
	}
}

void ShoutEventHandler::onBeforeDespawn(NpcAI& npcAI) {
	if (npcAI.ask(poll::AIQuestion::CAN_SHOUT)) {
		Npc& npc = npcAI.getOwner();
		std::optional<std::vector<const NpcShout*>> shouts = dataholders::DataManager::NPC_SHOUT_DATA->getNpcShouts(
			npc.getPosition()->getMapId(), npc.getNpcId(), ShoutEventType::BEFORE_DESPAWN);
		NpcShoutsService::getInstance().shoutRandom(npc, nullptr, orEmpty(shouts), 0);
		NpcShoutsService::getInstance().removeShoutCooldown(npc);
	}
}

void ShoutEventHandler::onReachedWalkPoint(NpcAI& npcAI) {
	if (npcAI.ask(poll::AIQuestion::CAN_SHOUT)) {
		Npc& npc = npcAI.getOwner();
		std::optional<std::string> walkerId = npc.getSpawn()->getWalkerId();
		if (!walkerId)
			return;
		ShoutEventType shoutType =
			npc.getMoveController()->isChangingDirection() ? ShoutEventType::WALK_DIRECTION : ShoutEventType::WALK_WAYPOINT;
		std::optional<std::vector<const NpcShout*>> shouts =
			dataholders::DataManager::NPC_SHOUT_DATA->getNpcShouts(npc.getPosition()->getMapId(), npc.getNpcId(), shoutType);
		if (!shouts || shouts->empty()) {
			const model::templates::walker::WalkerTemplate* tp = dataholders::DataManager::WALKER_DATA->getWalkerTemplate(*walkerId);
			int32_t stepCount = static_cast<int32_t>(tp->getRouteSteps().size());
			if (Rnd::nextInt(stepCount) < 2) {
				if (runtime::Ptr<Player> player = runtime::as<Player>(npc.getTarget()))
					NpcShoutsService::getInstance().shoutRandom(npc, player, orEmpty(shouts), 0);
				else
					NpcShoutsService::getInstance().shoutRandom(npc, nullptr, orEmpty(shouts), 0);
			}
		}
	}
}

void ShoutEventHandler::onSwitchedTarget(NpcAI& npcAI, Creature& target) {
	if (runtime::Ptr<Player> player = runtime::as<Player>(target); player && npcAI.ask(poll::AIQuestion::CAN_SHOUT)) {
		Npc& npc = npcAI.getOwner();
		std::optional<std::vector<const NpcShout*>> shouts = dataholders::DataManager::NPC_SHOUT_DATA->getNpcShouts(
			npc.getPosition()->getMapId(), npc.getNpcId(), ShoutEventType::SWITCH_TARGET);
		NpcShoutsService::getInstance().shoutRandom(npc, player, orEmpty(shouts), 0);
	}
}

void ShoutEventHandler::onDied(NpcAI& npcAI) {
	if (npcAI.ask(poll::AIQuestion::CAN_SHOUT)) {
		Npc& npc = npcAI.getOwner();
		std::optional<std::vector<const NpcShout*>> shouts =
			dataholders::DataManager::NPC_SHOUT_DATA->getNpcShouts(npc.getPosition()->getMapId(), npc.getNpcId(), ShoutEventType::DIED);
		NpcShoutsService::getInstance().shoutRandom(npc, nullptr, orEmpty(shouts), 0);
	}
}

void ShoutEventHandler::onAttackBegin(NpcAI& npcAI) {
	if (npcAI.ask(poll::AIQuestion::CAN_SHOUT)) {
		Npc& npc = npcAI.getOwner();
		std::optional<std::vector<const NpcShout*>> shouts = dataholders::DataManager::NPC_SHOUT_DATA->getNpcShouts(
			npc.getPosition()->getMapId(), npc.getNpcId(), ShoutEventType::ATTACK_BEGIN);
		NpcShoutsService::getInstance().shoutRandom(npc, nullptr, orEmpty(shouts), 0);
	}
}

void ShoutEventHandler::onEnemyAttack(NpcAI& npcAI, Creature& attacker) {
	// TODO: [RR] change AI or randomize behavior for "cowards" and "fanatics" ???
	// TODO: Figure out what the difference between ATTACK_BEGIN and HELP; HELPCALL should make NPC run
	if (npcAI.ask(poll::AIQuestion::CAN_SHOUT)) {
		Npc& npc = npcAI.getOwner();
		// Java reads attacker.getActingCreature() twice (the instanceof and each cast, ShoutEventHandler.java:103-111); one read here, because
		// a Summon's acting creature is its master field and two reads could disagree
		if (runtime::Ptr<Player> actingPlayer = runtime::as<Player>(attacker.getActingCreature())) {
			if (npc.getAttackedCount() == 0) {
				std::optional<std::vector<const NpcShout*>> shouts = dataholders::DataManager::NPC_SHOUT_DATA->getNpcShouts(
					npc.getPosition()->getMapId(), npc.getNpcId(), ShoutEventType::ATTACKED);
				if (shouts && !shouts->empty()) {
					NpcShoutsService::getInstance().shoutRandom(npc, actingPlayer, orEmpty(shouts), 0);
					return;
				}
				shouts = dataholders::DataManager::NPC_SHOUT_DATA->getNpcShouts(
					npc.getPosition()->getMapId(), npc.getNpcId(), ShoutEventType::HELPCALL);
				NpcShoutsService::getInstance().shoutRandom(npc, actingPlayer, orEmpty(shouts), 0);
			}
		} else {
			std::optional<std::vector<const NpcShout*>> shouts = dataholders::DataManager::NPC_SHOUT_DATA->getNpcShouts(
				npc.getPosition()->getMapId(), npc.getNpcId(), ShoutEventType::ATTACKED);
			if (shouts && !shouts->empty()) {
				const NpcShout* shout = *Rnd::get(*shouts);
				NpcShoutsService::getInstance().shout(runtime::Ptr<Npc>(npc), nullptr, shout, shout->getPollDelay() / 1000);
			}
		}
	}
}

void ShoutEventHandler::onCast(NpcAI& npcAI, runtime::Ptr<Creature> firstTarget) {
	// Java: firstTarget instanceof Player - false for a null first target (header request m5b2-p2-8)
	if (runtime::Ptr<Player> player = runtime::as<Player>(firstTarget); player && npcAI.ask(poll::AIQuestion::CAN_SHOUT))
		handleNumericEvent(npcAI, *player, ShoutEventType::CAST_K);
}

void ShoutEventHandler::onAttack(NpcAI& npcAI, Creature& attacked) {
	if (runtime::Ptr<Player> player = runtime::as<Player>(attacked); player && npcAI.ask(poll::AIQuestion::CAN_SHOUT))
		handleNumericEvent(npcAI, *player, ShoutEventType::ATTACK_K);
}

void ShoutEventHandler::handleNumericEvent(NpcAI& npcAI, Player& creature, ShoutEventType eventType) {
	Npc& npc = npcAI.getOwner();
	std::optional<std::vector<const NpcShout*>> shouts =
		dataholders::DataManager::NPC_SHOUT_DATA->getNpcShouts(npc.getPosition()->getMapId(), npc.getNpcId(), eventType);
	if (!shouts || shouts->empty())
		return;

	std::vector<const NpcShout*> validShouts;
	std::vector<const NpcShout*> nonNumberedShouts;
	for (const NpcShout* shout : *shouts) {
		if (shout->getSkillNo() == 0)
			nonNumberedShouts.push_back(shout);
		else if (shout->getSkillNo() == npc.getSkillNumber())
			validShouts.push_back(shout);
	}

	NpcShoutsService::getInstance().shoutRandom(npc, runtime::Ptr<Player>(creature), !validShouts.empty() ? validShouts : nonNumberedShouts, 0);
}

void ShoutEventHandler::onAttackEnd(NpcAI& npcAI) {
	if (npcAI.ask(poll::AIQuestion::CAN_SHOUT)) {
		Npc& npc = npcAI.getOwner();
		std::optional<std::vector<const NpcShout*>> shouts = dataholders::DataManager::NPC_SHOUT_DATA->getNpcShouts(
			npc.getPosition()->getMapId(), npc.getNpcId(), ShoutEventType::ATTACK_END);
		NpcShoutsService::getInstance().shoutRandom(npc, nullptr, orEmpty(shouts), 0);
	}
}

} // namespace aion::gameserver::ai::handler
