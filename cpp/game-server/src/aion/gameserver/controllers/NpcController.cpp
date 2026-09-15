#include "aion/gameserver/controllers/NpcController.h"

#include <string>
#include <vector>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/ai/NpcAI.h"
#include "aion/gameserver/ai/event/AIEventType.h"
#include "aion/gameserver/ai/poll/AIQuestion.h"
#include "aion/gameserver/controllers/ControllerStandIns.h"
#include "aion/gameserver/controllers/ControllerSupport.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/controllers/attack/DamageInfo.h"
#include "aion/gameserver/controllers/attack/DamageList.h"
#include "aion/gameserver/controllers/attack/TeamDamageList.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/instance/handlers/InstanceHandler.h"
#include "aion/gameserver/model/drop/DropItem.h"
#include "aion/gameserver/model/gameobjects/AionObject.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/DropNpc.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/Pet.h"
#include "aion/gameserver/model/gameobjects/PetSpecialFunction.h"
#include "aion/gameserver/model/gameobjects/Summon.h"
#include "aion/gameserver/model/gameobjects/player/PetCommonData.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/player/Rates.h"
#include "aion/gameserver/model/gameobjects/state/CreatureState.h"
#include "aion/gameserver/model/geometry/Area.h"
#include "aion/gameserver/model/stats/container/NpcGameStats.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/model/team/TemporaryPlayerTeam.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_LOOKATOBJECT.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PET.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/runtime/collections/HashSet.h"
#include "aion/gameserver/services/DialogService.h"
#include "aion/gameserver/services/RespawnService.h"
#include "aion/gameserver/services/abyss/AbyssPointsService.h"
#include "aion/gameserver/services/drop/DropRegistrationService.h"
#include "aion/gameserver/services/drop/DropService.h"
#include "aion/gameserver/services/event/EventService.h"
#include "aion/gameserver/services/instance/InstanceScaler.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/taskmanager/tasks/MoveTaskManager.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/geo/GeoService.h"
#include "aion/gameserver/world/zone/ZoneInstance.h"

namespace aion::gameserver::controllers {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.controllers.NpcController");

using ai::event::AIEventType;
using ai::poll::AIQuestion;
using model::gameobjects::AionObject;
using model::gameobjects::Creature;
using model::gameobjects::Npc;
using model::gameobjects::Pet;
using model::gameobjects::VisibleObject;
using model::gameobjects::player::Player;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using runtime::Ptr;
using runtime::Ref;
using utils::PacketSendUtility;
using utils::PositionUtil;

NpcController::NpcController() = default;

NpcController::~NpcController() = default;

model::gameobjects::Npc& NpcController::getOwner() const {
	return static_cast<model::gameobjects::Npc&>(CreatureController::getOwner());
}

void NpcController::see(model::gameobjects::VisibleObject& object) {
	CreatureController::see(object);
	if (Ptr<Creature> creature = runtime::as<Creature>(object)) {
		getOwner().getAi().onCreatureEvent(AIEventType::CREATURE_SEE, *creature);
	}
}

void NpcController::notSee(model::gameobjects::VisibleObject& object, model::animations::ObjectDeleteAnimation animation) {
	if (Ptr<Creature> creature = runtime::as<Creature>(object)) {
		getOwner().getAi().onCreatureEvent(AIEventType::CREATURE_NOT_SEE, *creature);
	}
	CreatureController::notSee(object, animation);
}

void NpcController::onTargetChanged(runtime::Ptr<model::gameobjects::VisibleObject> oldTarget,
	runtime::Ptr<model::gameobjects::VisibleObject> newTarget) {
	CreatureController::onTargetChanged(oldTarget, newTarget);
	getOwner().clearAttackedCount();
	getOwner().getGameStats()->renewLastChangeTargetTime();
	if (!getOwner().isDead()) {
		if (!newTarget && getOwner().getObjectTemplate()->getTalkInfo() != nullptr) {
			// Java: lambda capturing this (fieldmap callback NpcController@L84:46), pinned to the controller part (retains the Npc)
			utils::ThreadPoolManager::getInstance().schedule(this, [this] {
				if (!getOwner().getTarget())
					getOwner().getAi().think(); // resume walking or reset heading
			}, 750);
		} else if (newTarget && !getOwner().equals(*newTarget)) {
			getOwner().getPosition()->setH(PositionUtil::getHeadingTowards(getOwner(), *newTarget));
		}
		// broadcast NPC's current target and heading
		PacketSendUtility::broadcastPacket(getOwner(), network::aion::serverpackets::SM_LOOKATOBJECT(getOwner()));
	}
}

void NpcController::onBeforeSpawn() {
	CreatureController::onBeforeSpawn();
	Npc& npc = getOwner();

	if (npc.getSpawn()->getState() > 0)
		npc.setState(npc.getSpawn()->getState());
	else if (npc.getObjectTemplate()->getState() > 0)
		npc.setState(npc.getObjectTemplate()->getState());
	else {
		npc.setState(model::gameobjects::state::CreatureState::WALK_MODE);
		if (npc.getSpawn()->isAerialSpawn())
			npc.setState(model::gameobjects::state::CreatureState::FLYING);
	}

	services::instance::InstanceScaler::onBeforeSpawn(npc);
	npc.getLifeStats()->setCurrentHpPercent(100);
	npc.getAi().onGeneralEvent(AIEventType::BEFORE_SPAWNED);
}

void NpcController::onAfterSpawn() {
	CreatureController::onAfterSpawn();
	getOwner().getAi().onGeneralEvent(AIEventType::SPAWNED);
}

void NpcController::onDespawn() {
	Npc& npc = getOwner();
	cancelCurrentSkill(nullptr);
	npc.getEffectController()->removeAllEffects();
	if (npc.getSpawn()->hasPool() && !npc.isDead())
		npc.getSpawn()->resetPoolSpot(npc.getInstanceId());
	services::drop::DropService::getInstance().unregisterDrop(npc);
	npc.getPosition()->getWorldMapInstance()->getInstanceHandler()->onDespawn(npc);
	npc.getAi().onGeneralEvent(AIEventType::DESPAWNED);
	getOwner().getObserveController()->clear();
	CreatureController::onDespawn();
}

void NpcController::onDie(model::gameobjects::Creature& lastAttacker) {
	Npc& npc = getOwner();
	if (npc.getSpawn()->hasPool())
		npc.getSpawn()->resetPoolSpot(npc.getInstanceId());

	if (npc.getAi().ask(AIQuestion::ALLOW_RESPAWN))
		services::RespawnService::scheduleRespawn(getOwner()); // schedule respawn before onDie events are fired, so handlers can cancel the respawn task if needed

	bool allowDecay = true;
	bool shouldLoot = true;
	try {
		allowDecay = npc.getAi().ask(AIQuestion::ALLOW_DECAY);
		shouldLoot = npc.getAi().ask(AIQuestion::REWARD_LOOT);
		if (npc.getAi().ask(AIQuestion::REWARD_AP_XP_DP_LOOT))
			doReward();
		npc.getPosition()->getWorldMapInstance()->getInstanceHandler()->onDie(npc);
		npc.getAi().onGeneralEvent(AIEventType::DIED);
	} catch (const std::exception& e) {
		log.error("onDie() exception for " + npc.toString() + ":", e);
	}

	CreatureController::onDie(lastAttacker);

	if (allowDecay) {
		if (shouldLoot)
			petLoot(npc);
		services::RespawnService::scheduleDecayTask(npc);
		if (getOwner().getSpawn() && getOwner().getSpawn()->getStaticId() > 0) {
			world::geo::GeoService::getInstance().despawnPlaceableObject(getOwner().getWorldId(), getOwner().getInstanceId(),
				getOwner().getSpawn()->getStaticId());
		}
	} else { // instant despawn (no decay time = no loot)
		delete_();
	}
}

void NpcController::petLoot(model::gameobjects::Npc& value) {
	Ptr<Pet> lootingPet = findPetForLooting(value);
	if (lootingPet && PositionUtil::isInRange(value, *lootingPet->getMaster(), 28, false)) {
		int32_t npcObjId = value.getObjectId();
		Ptr<runtime::RcHashSet<Ref<model::drop::DropItem>>> drops =
			services::drop::DropRegistrationService::getInstance().getCurrentDropMap().get(npcObjId);
		if (drops && !drops->isEmpty()) {
			PacketSendUtility::sendPacket(*lootingPet->getMaster(),
				network::aion::serverpackets::SM_PET(model::gameobjects::PetSpecialFunction::AUTOLOOT, true, npcObjId));
			for (const Ptr<model::drop::DropItem>& dropItem : drops->snapshot()) // array copy since the drops get removed on retrieval
				services::drop::DropService::getInstance().requestDropItem(*lootingPet->getMaster(), npcObjId, dropItem->getIndex(), true);
			PacketSendUtility::sendPacket(*lootingPet->getMaster(),
				network::aion::serverpackets::SM_PET(model::gameobjects::PetSpecialFunction::AUTOLOOT, false, npcObjId));
		}
	}
}

runtime::Ptr<model::gameobjects::Pet> NpcController::findPetForLooting(model::gameobjects::Npc& npc) {
	Ptr<model::gameobjects::DropNpc> dropNpc = services::drop::DropRegistrationService::getInstance().getDropRegistrationMap().get(npc.getObjectId());
	if (!dropNpc) // npc didn't drop anything
		return nullptr;
	Ptr<runtime::RcHashSet<int32_t>> allowedLooters = dropNpc->getAllowedLooters();
	if (allowedLooters->size() != 1) // auto looting is not available in FFA loot mode
		return nullptr;
	Ptr<Player> player = world::World::getInstance().getPlayer(allowedLooters->iterator().next());
	if (!player) // looter got disconnected
		return nullptr;
	Ptr<Pet> pet = player->getPet();
	return pet && pet->getCommonData()->isLooting() ? pet : nullptr;
}

void NpcController::doReward() {
	CreatureController::doReward();
	attack::TeamDamageList finalList = getOwner().getAggroList().getFinalDamageList().toTeamDamages();
	std::optional<attack::DamageInfo> mostDamage = finalList.getMostDamage();
	Ptr<AionObject> winner = !mostDamage ? nullptr : mostDamage->getAttacker();
	if (!winner)
		return;

	Ptr<instance::handlers::InstanceHandler> instanceHandler = getOwner().getPosition()->getWorldMapInstance()->getInstanceHandler();
	float apMultiplier = instanceHandler->getApMultiplier();
	for (const attack::DamageInfo& info : finalList.getCreatureOrTeamDamages()) {
		Ptr<AionObject> attacker = info.getAttacker();
		float percentage = static_cast<float>(info.getDamage()) / static_cast<float>(finalList.getTotalDamage());
		if (Ptr<model::team::TemporaryPlayerTeam> tmpPlayerTeam = runtime::as<model::team::TemporaryPlayerTeam>(attacker)) {
			standins::playerTeamDistributionServiceDoReward(*tmpPlayerTeam, percentage, getOwner(), *winner, finalList);
		} else if (Ptr<Player> player = runtime::as<Player>(attacker)) {
			if (!player->isDead()) {
				// Reward init
				int64_t rewardXp = standins::statFunctionsCalculateExperienceReward(player->getLevel(), getOwner());
				int32_t rewardDp = standins::statFunctionsCalculateDPReward(*player, getOwner());
				float rewardAp = 1;

				// Dmg percent correction
				rewardXp = detail::toLong(static_cast<float>(rewardXp) * percentage);
				rewardDp = detail::toInt(static_cast<float>(rewardDp) * percentage);
				rewardAp *= percentage;
				rewardAp *= apMultiplier;

				bool shouldNotifyQuestEngine = !standins::isPvpMapHandler(*instanceHandler); // do not include pvp map
				if (shouldNotifyQuestEngine) {
					Ref<questEngine::model::QuestEnv> env = questEngine::model::QuestEnv::create(getOwner(), *player, 0);
					questEngine::QuestEngine::getInstance().onKill(*env);
				}
				services::event::EventService::getInstance().onPveKill(*player, getOwner());
				player->getCommonData()->addExp(rewardXp, model::gameobjects::player::Rates::XP_HUNTING, getOwner().getObjectTemplate()->getL10n());
				player->getCommonData()->addDp(rewardDp);
				if (getOwner().getAi().ask(AIQuestion::REWARD_AP)) {
					int32_t calculatedAp = standins::statFunctionsCalculatePvEApGained(*player, getOwner());
					rewardAp *= static_cast<float>(calculatedAp);
					if (rewardAp >= 1) {
						services::abyss::AbyssPointsService::addAp(*player, getOwner(), detail::toInt(rewardAp));
					}
				}
			}
			if (attacker->equals(*winner) && getOwner().getAi().ask(AIQuestion::REWARD_LOOT))
				// Java passes null groupMembers; the frozen signature takes a vector, an empty one stands for null
				services::drop::DropRegistrationService::getInstance().registerDrop(getOwner(), *player, player->getLevel(), {});
		}
	}
}

void NpcController::onDialogRequest(model::gameobjects::player::Player& player) {
	// notify npc dialog request observer
	if (!getOwner().getObjectTemplate()->canInteract())
		return;
	if (!PositionUtil::isInTalkRange(player, getOwner())) {
		if (getOwner().getObjectTemplate()->isDialogNpc())
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_DIALOG_TOO_FAR_TO_TALK());
		else
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_WAREHOUSE_TOO_FAR_FROM_NPC());
		return;
	}

	getOwner().getAi().onCreatureEvent(AIEventType::DIALOG_START, player);
}

void NpcController::onDialogSelect(int32_t dialogActionId, int32_t prevDialogId, model::gameobjects::player::Player& player, int32_t questId,
	int32_t extendedRewardIndex) {
	if (!PositionUtil::isInTalkRange(player, getOwner()))
		return;
	if (!getOwner().getAi().onDialogSelect(player, dialogActionId, questId, extendedRewardIndex)) {
		services::DialogService::onDialogSelect(dialogActionId, player, getOwner(), questId, extendedRewardIndex);
	}
}

void NpcController::onAddHate(model::gameobjects::Creature& attacker, bool isNewInAggroList) {
	if (Ptr<Player> attackingPlayer = runtime::as<Player>(attacker); isNewInAggroList && attackingPlayer) {
		if (attackingPlayer->isInTeam()) {
			for (const Ptr<AionObject>& member :
				attackingPlayer->getCurrentTeam()->filterMembers([this](AionObject& m) { return PositionUtil::isInRange(getOwner(), *runtime::cast<Player>(m), 50); })) {
				Ref<questEngine::model::QuestEnv> env = questEngine::model::QuestEnv::create(getOwner(), *runtime::cast<Player>(member), 0);
				questEngine::QuestEngine::getInstance().onAddAggroList(*env);
			}
		} else {
			Ref<questEngine::model::QuestEnv> env = questEngine::model::QuestEnv::create(getOwner(), *attackingPlayer, 0);
			questEngine::QuestEngine::getInstance().onAddAggroList(*env);
		}
	}
	CreatureController::onAddHate(attacker, isNewInAggroList);
}

void NpcController::onAttack(model::gameobjects::Creature& attacker, runtime::Ptr<skillengine::model::Effect> effect,
	network::aion::serverpackets::SM_ATTACK_STATUS_TYPE type, int32_t damage, bool notifyAttack,
	network::aion::serverpackets::SM_ATTACK_STATUS_LOG logId, std::optional<attack::AttackStatus> attackStatus,
	std::optional<skillengine::model::HopType> hopType) {
	if (getOwner().isDead())
		return;
	Ptr<Creature> actingCreature;

	// summon should gain its own aggro (except if despawned, for example because of a damage over time effect)
	if (runtime::as<model::gameobjects::Summon>(attacker) && attacker.isSpawned())
		actingCreature = attacker;
	else
		actingCreature = attacker.getActingCreature();

	CreatureController::onAttack(*actingCreature, effect, type, damage, notifyAttack, logId, attackStatus, hopType);

	Npc& npc = getOwner();
	standins::shoutEventHandlerOnEnemyAttack(*runtime::cast<ai::NpcAI>(npc.getAi()), attacker);
	if (Ptr<Player> player = runtime::as<Player>(actingCreature)) {
		Ref<questEngine::model::QuestEnv> env = questEngine::model::QuestEnv::create(npc, *player, 0);
		questEngine::QuestEngine::getInstance().onAttack(*env);
	}
}

void NpcController::onStartMove() {
	CreatureController::onStartMove();
	taskmanager::tasks::MoveTaskManager::getInstance().addCreature(getOwner());
}

void NpcController::onStopMove() {
	CreatureController::onStopMove();
	taskmanager::tasks::MoveTaskManager::getInstance().removeCreature(getOwner());
}

void NpcController::onEnterZone(world::zone::ZoneInstance& zoneInstance) {
	if (zoneInstance.getAreaTemplate()->getZoneName() == nullptr) {
		log.error("No name found for a Zone in the map " + std::to_string(zoneInstance.getAreaTemplate()->getWorldId()));
	}
}

bool NpcController::useSkill(int32_t skillId, int32_t skillLevel) {
	const skillengine::model::SkillTemplate* skillTemplate = dataholders::DataManager::SKILL_DATA->getSkillTemplate(skillId);
	if (!getOwner().isSkillDisabled(skillTemplate)) {
		getOwner().getGameStats()->renewLastSkillTime();
		return CreatureController::useSkill(skillId, skillLevel);
	}
	return false;
}

void NpcController::loseAggro(bool restoreHp) {
	getOwner().setTarget(nullptr);
	getOwner().getAggroList().clear();
	if (restoreHp)
		getOwner().getLifeStats()->triggerRestoreTask();
}

} // namespace aion::gameserver::controllers
