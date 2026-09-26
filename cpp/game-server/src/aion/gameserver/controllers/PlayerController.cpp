#include "aion/gameserver/controllers/PlayerController.h"

#include <algorithm>
#include <any>
#include <cstdint>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/GameServer.h"
#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/ai/NpcAI.h"
#include "aion/gameserver/ai/handler/ShoutEventHandler.h"
#include "aion/gameserver/configs/administration/AdminConfig.h"
#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/configs/main/GSConfig.h"
#include "aion/gameserver/configs/main/HTMLConfig.h"
#include "aion/gameserver/configs/main/MembershipConfig.h"
#include "aion/gameserver/configs/main/SecurityConfig.h"
#include "aion/gameserver/controllers/ControllerStandIns.h"
#include "aion/gameserver/controllers/ControllerSupport.h"
#include "aion/gameserver/controllers/FlyController.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/controllers/attack/AttackUtil.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/controllers/effect/PlayerEffectController.h"
#include "aion/gameserver/instance/handlers/InstanceHandler.h"
#include "aion/gameserver/model/animations/ObjectDeleteAnimation.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/controllers/observer/StanceObserver.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/PanelSkillsData.h"
#include "aion/gameserver/dataholders/QuestsData.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/EmotionType.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/actions/PlayerMode.h"
#include "aion/gameserver/model/animations/ActionAnimation.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Gatherable.h"
#include "aion/gameserver/model/gameobjects/HouseObject.h"
#include "aion/gameserver/model/gameobjects/Kisk.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/Pet.h"
#include "aion/gameserver/model/gameobjects/PetEmote.h"
#include "aion/gameserver/model/gameobjects/StaticObject.h"
#include "aion/gameserver/model/gameobjects/Summon.h"
#include "aion/gameserver/model/gameobjects/TransformModel.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/player/motion/Motion.h"
#include "aion/gameserver/model/gameobjects/player/motion/MotionList.h"
#include "aion/gameserver/model/gameobjects/player/npcFaction/NpcFactions.h"
#include "aion/gameserver/model/gameobjects/state/CreatureState.h"
#include "aion/gameserver/model/gameobjects/state/CreatureVisualState.h"
#include "aion/gameserver/model/gameobjects/state/FlyState.h"
#include "aion/gameserver/model/geometry/Area.h"
#include "aion/gameserver/model/house/House.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/model/summons/UnsummonType.h"
#include "aion/gameserver/model/templates/QuestTemplate.h"
#include "aion/gameserver/model/templates/flypath/FlightPath_Type.h"
#include "aion/gameserver/model/templates/flypath/FlyPathEntry.h"
#include "aion/gameserver/model/templates/housing/HouseAddress.h"
#include "aion/gameserver/model/templates/housing/HouseType.h"
#include "aion/gameserver/model/templates/panels/SkillPanel.h"
#include "aion/gameserver/model/templates/ride/RideInfo.h"
#include "aion/gameserver/model/templates/zone/ZoneTemplate.h"
#include "aion/gameserver/model/templates/zone/ZoneType.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ABNORMAL_EFFECT.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ACTION_ANIMATION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_RESPONSE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DELETE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DELETE_HOUSE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DELETE_HOUSE_OBJECT.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_GATHERABLE_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_HOUSE_OBJECT.h"
#include "aion/gameserver/network/aion/serverpackets/SM_HOUSE_RENDER.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ITEM_USAGE_ANIMATION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_KISK_UPDATE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_NEARBY_QUESTS.h"
#include "aion/gameserver/network/aion/serverpackets/SM_NPC_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PET.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PET_EMOTE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PLAYER_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PLAYER_STANCE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PLAYER_STATE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PRIVATE_STORE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUEST_REPEAT.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SKILL_CANCEL.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_TARGET_SELECTED.h"
#include "aion/gameserver/network/aion/serverpackets/SM_TARGET_UPDATE.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/restrictions/PlayerRestrictions.h"
#include "aion/gameserver/services/BonusPackService.h"
#include "aion/gameserver/services/DuelService.h"
#include "aion/gameserver/services/FactionPackService.h"
#include "aion/gameserver/services/HTMLService.h"
#include "aion/gameserver/services/LegionService.h"
#include "aion/gameserver/services/PvpService.h"
#include "aion/gameserver/services/QuestService.h"
#include "aion/gameserver/services/RecallService.h"
#include "aion/gameserver/services/RecallService_CancelReason.h"
#include "aion/gameserver/services/SkillLearnService.h"
#include "aion/gameserver/services/conquerorAndProtectorSystem/ConquerorAndProtectorService.h"
#include "aion/gameserver/services/drop/DropService.h"
#include "aion/gameserver/services/instance/InstanceService.h"
#include "aion/gameserver/services/reward/StarterKitService.h"
#include "aion/gameserver/services/summons/SummonsService.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/effect/EffectTemplate.h"
#include "aion/gameserver/skillengine/effect/RebirthEffect.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/skillengine/model/Skill_SkillMethod.h"
#include "aion/gameserver/taskmanager/tasks/PlayerMoveTaskManager.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"
#include "aion/gameserver/world/MapRegion.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldMapTypeInfo.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/WorldType.h"
#include "aion/gameserver/world/geo/GeoService.h"
#include "aion/gameserver/world/knownlist/KnownList.h"
#include "aion/gameserver/world/zone/ZoneInstance.h"

namespace aion::gameserver::controllers {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.controllers.PlayerController");

using ai::handler::ShoutEventHandler;
using model::actions::PlayerMode;
using model::gameobjects::Creature;
using model::gameobjects::Npc;
using model::gameobjects::Pet;
using model::gameobjects::VisibleObject;
using model::gameobjects::player::Player;
using model::gameobjects::state::CreatureState;
using model::gameobjects::state::FlyState;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using restrictions::PlayerRestrictions;
using runtime::Ptr;
using runtime::Ref;
using services::RecallService_CancelReason;
using skillengine::model::Effect;
using skillengine::model::Skill;
using utils::PacketSendUtility;
using utils::PositionUtil;

PlayerController::PlayerController() = default;

PlayerController::~PlayerController() = default;

model::gameobjects::player::Player& PlayerController::getOwner() const {
	return static_cast<model::gameobjects::player::Player&>(CreatureController::getOwner());
}

void PlayerController::see(model::gameobjects::VisibleObject& object) {
	CreatureController::see(object);
	Player& player = getOwner();
	if (Ptr<Creature> creature = runtime::as<Creature>(object)) {
		if (Ptr<Npc> npc = runtime::as<Npc>(creature)) {
			PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_NPC_INFO(*npc, player));
			if (Ptr<model::gameobjects::Kisk> kisk = runtime::as<model::gameobjects::Kisk>(npc)) {
				if (player.getRace() == kisk->getOwnerRace())
					PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_KISK_UPDATE(*kisk));
			} else {
				Ref<questEngine::model::QuestEnv> env = questEngine::model::QuestEnv::create(npc, player, 0);
				questEngine::QuestEngine::getInstance().onAtDistance(*env);
			}
			services::drop::DropService::getInstance().see(player, *npc);
		} else if (Ptr<Player> other = runtime::as<Player>(creature)) {
			sendPlayerInfoPackets(*other);
		} else if (Ptr<model::gameobjects::Summon> summon = runtime::as<model::gameobjects::Summon>(creature)) {
			PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_NPC_INFO(*summon, player));
		}
		if (!creature->getEffectController()->isEmpty())
			PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_ABNORMAL_EFFECT(*creature));
	} else if (runtime::as<model::gameobjects::Gatherable>(object) || runtime::as<model::gameobjects::StaticObject>(object)) {
		PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_GATHERABLE_INFO(object));
	} else if (Ptr<Pet> pet = runtime::as<Pet>(object)) {
		PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_PET(*pet));
		if (pet->getMaster()->isInFlyingState())
			PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_PET_EMOTE(*pet, model::gameobjects::PetEmote::FLY_START));
	} else if (Ptr<model::house::House> house = runtime::as<model::house::House>(object)) {
		PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_HOUSE_RENDER(*house));
	} else if (Ptr<model::gameobjects::HouseObject> houseObject = runtime::as<model::gameobjects::HouseObject>(object)) {
		PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_HOUSE_OBJECT(*houseObject));
	}
}

void PlayerController::sendPlayerInfoPackets(model::gameobjects::player::Player& player) {
	Player& self = getOwner();
	PacketSendUtility::sendPacket(self, network::aion::serverpackets::SM_PLAYER_INFO(player, !player.equals(self) && self.isAggroIconTo(player)));
	std::unordered_map<int32_t, Ptr<model::gameobjects::player::motion::Motion>> activeMotions;
	if (Ptr<runtime::RcLinkedHashMap<int32_t, Ref<model::gameobjects::player::motion::Motion>>> motions = player.getMotions().getActiveMotions()) {
		for (const auto& entry : motions->entrySet())
			activeMotions.emplace(entry.getKey(), entry.getValue());
	}
	PacketSendUtility::sendPacket(self, network::aion::serverpackets::SM_MOTION(player.getObjectId(), activeMotions));
	if (player.isInPlayerMode(PlayerMode::RIDE))
		PacketSendUtility::sendPacket(self, network::aion::serverpackets::SM_EMOTION(player, model::EmotionType::RIDE, 0, player.ride.get()->getNpcId()));
	if (player.getController().isUnderStance())
		PacketSendUtility::sendPacket(self, network::aion::serverpackets::SM_PLAYER_STANCE(player, 1));
}

void PlayerController::notSee(model::gameobjects::VisibleObject& object, model::animations::ObjectDeleteAnimation animation) {
	CreatureController::notSee(object, animation);
	Player& player = getOwner();
	if (!player.isSpawned()) // player is teleporting, no need to send deletion packets
		return;
	Ptr<Npc> npc;
	if (runtime::as<Pet>(object)) {
		PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_PET(object.getObjectId(), animation));
	} else if (Ptr<model::house::House> house = runtime::as<model::house::House>(object)) {
		PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_DELETE_HOUSE(house->getAddress()->getId()));
	} else if (runtime::as<model::gameobjects::HouseObject>(object)) {
		PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_DELETE_HOUSE_OBJECT(object.getObjectId()));
	} else if ((npc = runtime::as<Npc>(object)) && npc->isFlag()) {
		PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_DELETE(object, model::animations::ObjectDeleteAnimation::DELAYED));
	} else {
		PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_DELETE(object, animation));
	}
}

void PlayerController::onTargetChanged(runtime::Ptr<model::gameobjects::VisibleObject> oldTarget,
	runtime::Ptr<model::gameobjects::VisibleObject> newTarget) {
	CreatureController::onTargetChanged(oldTarget, newTarget);
	PacketSendUtility::sendPacket(getOwner(), network::aion::serverpackets::SM_TARGET_SELECTED(newTarget));
	PacketSendUtility::broadcastToSightedPlayers(getOwner(), network::aion::serverpackets::SM_TARGET_UPDATE(getOwner()));
}

void PlayerController::onHide() {
	CreatureController::onHide();
	services::DuelService::getInstance().fixTeamVisibility(getOwner());
}

void PlayerController::onHideEnd() {
	Ptr<Pet> pet = getOwner().getPet();
	if (pet && !PositionUtil::isInRange(getOwner(), *pet, 3)) // client sends pet position only every 50m...
		pet->getPosition()->setXYZH(getOwner().getX(), getOwner().getY(), getOwner().getZ(), getOwner().getHeading());
	CreatureController::onHideEnd();
}

void PlayerController::updateNearbyQuests() {
	std::unordered_map<int32_t, int32_t> nearbyQuestList;
	for (int32_t questId : getOwner().getPosition()->getMapRegion()->getParent().getQuestIds()) {
		if (services::QuestService::checkStartConditions(getOwner(), questId, false, 2, false, false, false))
			nearbyQuestList[questId] = services::QuestService::getLevelRequirementDiff(questId, getOwner().getCommonData()->getLevel());
	}
	PacketSendUtility::sendPacket(getOwner(), network::aion::serverpackets::SM_NEARBY_QUESTS(nearbyQuestList));
}

void PlayerController::updateRepeatableQuests() {
	std::vector<int32_t> reapeatQuestList;
	for (int32_t questId : getOwner().getPosition()->getMapRegion()->getParent().getQuestIds()) {
		const model::templates::QuestTemplate* template_ =
			detail::nonNull(dataholders::DataManager::QUEST_DATA->getQuestById(questId), "QUEST_DATA.getQuestById(questId)");
		if (!template_->isTimeBased())
			continue;
		if (services::QuestService::checkStartConditions(getOwner(), questId, false))
			reapeatQuestList.push_back(questId);
	}
	if (reapeatQuestList.size() > 0)
		PacketSendUtility::sendPacket(getOwner(), network::aion::serverpackets::SM_QUEST_REPEAT(reapeatQuestList));
}

void PlayerController::onEnterZone(world::zone::ZoneInstance& zone) {
	Player& player = getOwner();
	if (!zone.canRide() && player.isInPlayerMode(PlayerMode::RIDE))
		player.unsetPlayerMode(PlayerMode::RIDE);
	services::conquerorAndProtectorSystem::ConquerorAndProtectorService::getInstance().onEnterZone(player, zone);
	services::instance::InstanceService::onEnterZone(player, zone);
	const world::zone::ZoneName* zoneName = zone.getAreaTemplate()->getZoneName();
	if (zoneName == nullptr)
		log.warn("No name found for a zone in map " + std::to_string(zone.getAreaTemplate()->getWorldId()) + " with xml name " +
			zone.getZoneTemplate()->getXmlName());
	else {
		Ref<questEngine::model::QuestEnv> env = questEngine::model::QuestEnv::create(nullptr, player, 0);
		questEngine::QuestEngine::getInstance().onEnterZone(*env, zoneName);
	}
}

void PlayerController::onLeaveZone(world::zone::ZoneInstance& zone) {
	Player& player = getOwner();
	services::conquerorAndProtectorSystem::ConquerorAndProtectorService::getInstance().onLeaveZone(player, zone);
	services::instance::InstanceService::onLeaveZone(player, zone);
	const world::zone::ZoneName* zoneName = zone.getAreaTemplate()->getZoneName();
	if (zoneName == nullptr)
		log.warn("No name found for a zone in map " + std::to_string(zone.getAreaTemplate()->getWorldId()) + " with xml name " +
			zone.getZoneTemplate()->getXmlName());
	else {
		Ref<questEngine::model::QuestEnv> env = questEngine::model::QuestEnv::create(nullptr, player, 0);
		questEngine::QuestEngine::getInstance().onLeaveZone(*env, zoneName);
	}
}

void PlayerController::onLeaveFlyArea() {
	Player& player = getOwner();
	if (!player.hasAccess(configs::administration::AdminConfig::FREE_FLIGHT)) {
		if (player.isInFlyingState()) {
			if (player.isInGlidingState()) {
				player.unsetFlyState(FlyState::FLYING);
				player.unsetState(CreatureState::FLYING);
				player.getLifeStats()->triggerFpReduce();
				player.getGameStats()->updateStatsAndSpeedVisually();
				PacketSendUtility::broadcastPacket(player, network::aion::serverpackets::SM_EMOTION(player, model::EmotionType::STOP_FLY), true);
			} else {
				player.getFlyController().endFly(true);
				if (player.isSpawned() && !player.isInsideZoneType(model::templates::zone::ZoneType::FLY)) // not spawned means leaving by teleporter
					utils::audit::AuditLogger::log(player, "left fly zone in fly state at " + player.getPosition()->toString());
			}
		} else if (player.isInGlidingState()) {
			player.getLifeStats()->triggerFpReduce();
		}
	}
}

void PlayerController::onEnterFlyArea() {
	getOwner().getLifeStats()->triggerFpReduce();
}

void PlayerController::onEnterWorld() {
	Player& player = getOwner();
	if (player.getPosition()->getWorldMapInstance()->getParent()->isExceptBuff()) {
		if (!standins::pvpMapServiceIsOnPvPMap(player))
			player.getEffectController()->removeAllEffects();
	}

	for (const Ptr<Effect>& ef : player.getEffectController()->getAbnormalEffects()) {
		if (ef->isDeityAvatar()) {
			// remove abyss transformation if worldtype != abyss && worldtype != balaurea && worldType != panesterra
			if ((player.getWorldType() != world::WorldType::ABYSS && player.getWorldType() != world::WorldType::BALAUREA &&
					player.getWorldType() != world::WorldType::PANESTERRA) ||
				player.isInInstance()) {
				ef->endEffect();
			}
		}
	}
}

void PlayerController::onDie(model::gameobjects::Creature& lastAttacker) {
	Player& player = getOwner();
	player.getController().cancelCurrentSkill(nullptr);
	services::RecallService::getInstance().cancel(player, RecallService_CancelReason::CANCELLED);
	setRebirthReviveInfo();
	Ptr<Creature> master = lastAttacker.getMaster();

	if (services::DuelService::getInstance().isDueling(player)) {
		bool killedByOpponent = player.isDueling(*master);
		services::DuelService::getInstance().loseDuel(player);
		if (killedByOpponent) {
			if (player.getLifeStats()->getHpPercentage() < 33)
				player.getLifeStats()->setCurrentHpPercent(33);
			if (player.getLifeStats()->getMpPercentage() < 33)
				player.getLifeStats()->setCurrentMpPercent(33);
			if (master->getLifeStats()->getHpPercentage() < 33)
				master->getLifeStats()->setCurrentHpPercent(33);
			if (master->getLifeStats()->getMpPercentage() < 33)
				master->getLifeStats()->setCurrentMpPercent(33);
			return;
		}
	}

	// Release summon
	Ptr<model::gameobjects::Summon> summon = player.getSummon();
	if (summon)
		services::summons::SummonsService::release(*summon, model::summons::UnsummonType::MASTER_DEATH);

	// setIsFlyingBeforeDead for PlayerReviveService
	if (player.isInState(CreatureState::FLYING))
		player.setIsFlyingBeforeDeath(true);

	// ride
	player.setPlayerMode(PlayerMode::RIDE, std::any());
	player.unsetState(CreatureState::RESTING);
	player.unsetState(CreatureState::FLOATING_CORPSE);

	// unset flying
	player.unsetState(CreatureState::FLYING);
	player.unsetState(CreatureState::GLIDING);
	player.unsetFlyState(FlyState::FLYING);
	player.unsetFlyState(FlyState::GLIDING);

	// Effects removed with super.onDie()
	CreatureController::onDie(lastAttacker);

	scheduleShowResurrectionOptions();

	if (player.getPosition()->getWorldMapInstance()->getInstanceHandler()->onDie(player, lastAttacker))
		return;

	Ptr<world::MapRegion> mapRegion = player.getPosition()->getMapRegion();
	if (mapRegion && mapRegion->onDie(lastAttacker, player))
		return;

	doReward();

	if (runtime::as<Npc>(master) || master->equals(player)) {
		if (player.getLevel() > 4 && !player.getEffectController()->hasAbnormalEffect([](Effect& effect) { return effect.isNoDeathPenalty(); }))
			player.getCommonData()->calculateExpLoss();
	}

	Ref<questEngine::model::QuestEnv> env = questEngine::model::QuestEnv::create(nullptr, player, 0);
	questEngine::QuestEngine::getInstance().onDie(*env);
}

void PlayerController::setRebirthReviveInfo() {
	Player& player = getOwner();
	// Store the effect info.
	std::vector<Ptr<Effect>> effects = player.getEffectController()->getAbnormalEffects();
	for (const Ptr<Effect>& effect : effects) {
		for (const skillengine::effect::EffectTemplate* template_ : effect->getEffectTemplates()) {
			const auto* rebirthEffect = dynamic_cast<const skillengine::effect::RebirthEffect*>(template_);
			if (template_->getEffectId() == 160 && rebirthEffect != nullptr) {
				player.setRebirthEffect(rebirthEffect);
				return;
			}
		}
	}
	player.setRebirthEffect(nullptr);
}

void PlayerController::onDespawn() {
	if (getOwner().isLooting())
		services::drop::DropService::getInstance().closeDropList(getOwner(), getOwner().getLootingNpcOid());
	CreatureController::onDespawn();
}

void PlayerController::scheduleShowResurrectionOptions() {
	// Java: lambda capturing this (fieldmap callback PlayerController@L356:44), pinned to the controller part (retains the Player)
	utils::ThreadPoolManager::getInstance().schedule(this, [this] {
		// teleportation task can be assigned shortly after death (see PlayerReviveService#scheduleReviveAtBase)
		if (getOwner().isDead() && !hasTask(model::TaskId::TELEPORT))
			showResurrectionOptions();
	}, 500);
}

void PlayerController::showResurrectionOptions() {
	PacketSendUtility::sendPacket(getOwner(), network::aion::serverpackets::SM_DIE(getOwner()));
}

bool PlayerController::isInvader(model::gameobjects::player::Player& player) {
	if (player.getRace() == model::Race::ASMODIANS) {
		return player.getWorldId() == 210060000;
	} else {
		return player.getWorldId() == 220050000;
	}
}

void PlayerController::doReward() {
	services::PvpService::getInstance().doReward(getOwner());
}

void PlayerController::onBeforeSpawn() {
	CreatureController::onBeforeSpawn();
	Player& player = getOwner();
	if (!player.isDead()) {
		if (player.getIsFlyingBeforeDeath())
			player.unsetState(CreatureState::FLOATING_CORPSE);
		else if (player.isInState(CreatureState::DEAD))
			player.unsetState(CreatureState::DEAD);
		player.setState(CreatureState::ACTIVE);
	}
	player.setHitTimeBoost(0, 0);
	if (player.getPanesterraFaction() && !world::isPanesterraMap(player.getWorldId()))
		player.setPanesterraFaction(std::nullopt);
}

void PlayerController::attackTarget(runtime::Ptr<model::gameobjects::Creature> target, int32_t time, bool skipChecks) {
	Player& player = getOwner();
	if (!PlayerRestrictions::canAttack(player, *target))
		return;

	Ptr<model::stats::container::PlayerGameStats> gameStats = player.getGameStats();
	// client allows attacking from +0.̅9 meters further away
	float attackRange = 1 + static_cast<float>(gameStats->getAttackRange()->getCurrent()) / 1000.0f;
	// client can send CM_ATTACK before in range (even before sending CM_MOVE). we only allow it on first hit to minimize exploit potential
	if (!target->getAggroList().isHating(player))
		attackRange += PositionUtil::calculateMaxCoveredDistance(player, 100);
	if (!PositionUtil::isInAttackRange(player, target, attackRange)) {
		PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_ATTACK_RESPONSE::TARGET_TOO_FAR_AWAY(gameStats->getAttackCounter()));
		return;
	}

	if (!world::geo::GeoService::getInstance().canSee(player, *target)) {
		PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_ATTACK_RESPONSE::STOP_OBSTACLE_IN_THE_WAY(gameStats->getAttackCounter()));
		return;
	}

	if (runtime::as<Npc>(target)) {
		Ref<questEngine::model::QuestEnv> env = questEngine::model::QuestEnv::create(target, player, 0);
		questEngine::QuestEngine::getInstance().onAttack(*env);
	}

	int32_t attackSpeed = gameStats->getAttackSpeed()->getCurrent();

	int64_t milis = commons::utils::currentTimeMillis();
	// network ping..
	if (milis - lastAttackMillis.get() + 300 < attackSpeed) {
		// hack
		PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_ATTACK_RESPONSE::STOP_WITHOUT_MESSAGE(gameStats->getAttackCounter()));
		return;
	}
	enterCombat(true);

	CreatureController::attackTarget(target, time, true);
}

void PlayerController::onAttack(model::gameobjects::Creature& attacker, runtime::Ptr<skillengine::model::Effect> effect,
	network::aion::serverpackets::SM_ATTACK_STATUS_TYPE type, int32_t damage, bool notifyAttack,
	network::aion::serverpackets::SM_ATTACK_STATUS_LOG logId, std::optional<attack::AttackStatus> attackStatus,
	std::optional<skillengine::model::HopType> hopType) {
	Player& player = getOwner();
	if (player.isDead())
		return;

	if (player.isProtectionActive())
		return;

	// avoid killing players after duel
	if (!player.equals(attacker) && runtime::as<Player>(attacker.getActingCreature()) && !player.isEnemy(attacker))
		return;

	cancelUseItem();
	CreatureController::onAttack(attacker, effect, type, damage, notifyAttack, logId, attackStatus, hopType);

	if (runtime::as<Npc>(attacker)) {
		ShoutEventHandler::onAttack(*runtime::cast<ai::NpcAI>(attacker.getAi()), player);
		Ref<questEngine::model::QuestEnv> env = questEngine::model::QuestEnv::create(attacker, player, 0);
		questEngine::QuestEngine::getInstance().onAttack(*env);
	}

	enterCombat(false);
}

void PlayerController::useSkill(const skillengine::model::SkillTemplate* template_, int32_t targetType, float x, float y, float z,
	int32_t clientHitTime, int32_t skillLevel) {
	Player& player = getOwner();
	Ref<Skill> skill = skillengine::SkillEngine::getInstance().getSkillFor(player, template_, player.getTarget());
	if (!skill && player.isTransformed()) {
		const model::templates::panels::SkillPanel* panel =
			dataholders::DataManager::PANEL_SKILL_DATA->getSkillPanel(player.getTransformModel().getPanelId());
		if (panel != nullptr && panel->canUseSkill(template_->getSkillId(), skillLevel)) {
			skill = skillengine::SkillEngine::getInstance().getSkillFor(player, template_, player.getTarget(), skillLevel);
		}
	}

	if (skill) {
		// item casts get interrupted by skill usage (skill casts don't, see PlayerRestrictions#canUseSkill). this must
		// happen before checking the restrictions, since Creature#canAttack returns false as long as we are casting
		if (player.isCasting() && player.getCastingSkill()->getItemTemplate() != nullptr)
			cancelCurrentSkill(nullptr);

		if (!PlayerRestrictions::canUseSkill(player, *skill))
			return;

		skill->setTargetType(targetType, x, y, z);
		skill->setClientHitTime(clientHitTime);
		skill->useSkill();
	}
}

void PlayerController::onStartMove() {
	CreatureController::onStartMove();
	taskmanager::tasks::PlayerMoveTaskManager::getInstance().addPlayer(getOwner());
	cancelUseItem();
	cancelCurrentSkill(nullptr);
}

void PlayerController::onMove() {
	CreatureController::onMove();
	if (getOwner().isInTeam())
		standins::teamMoveUpdaterAdd(getOwner());
}

void PlayerController::onStopMove() {
	CreatureController::onStopMove();
	taskmanager::tasks::PlayerMoveTaskManager::getInstance().removePlayer(getOwner());
	cancelCurrentSkill(nullptr);
	updateZone();
}

void PlayerController::notifyAIOnMove() {
	if (getOwner().isUsingFlightTransporterOrWindstream())
		return;
	CreatureController::notifyAIOnMove();
}

void PlayerController::cancelCurrentSkill(runtime::Ptr<model::gameobjects::Creature> lastAttacker) {
	SM_SYSTEM_MESSAGE message = SM_SYSTEM_MESSAGE::STR_SKILL_CANCELED();
	cancelCurrentSkill(lastAttacker, &message);
}

void PlayerController::cancelCurrentSkill(runtime::Ptr<model::gameobjects::Creature> lastAttacker,
	network::aion::serverpackets::SM_SYSTEM_MESSAGE* message) {
	if (!getOwner().getCastingSkill()) {
		return;
	}

	Player& player = getOwner();
	Ptr<Skill> castingSkill = player.getCastingSkill();
	castingSkill->cancelCast();
	player.setCasting(nullptr);
	if (castingSkill->allowAnimationBoostByCastSpeed())
		player.setHitTimeBoost(std::numeric_limits<int64_t>::max(), castingSkill->getCastSpeedForAnimationBoostAndChargeSkills()); // yes, this is retail client behavior
	else
		player.setHitTimeBoost(0, 0);
	if (castingSkill->getSkillMethod() == skillengine::model::Skill_SkillMethod::CAST) {
		PacketSendUtility::broadcastPacket(player, network::aion::serverpackets::SM_SKILL_CANCEL(player, castingSkill->getSkillTemplate()->getSkillId()), true);
		if (message != nullptr)
			PacketSendUtility::sendPacket(player, *message);
	} else if (castingSkill->getSkillMethod() == skillengine::model::Skill_SkillMethod::ITEM) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_ITEM_CANCELED());
		PacketSendUtility::broadcastPacket(player,
			network::aion::serverpackets::SM_ITEM_USAGE_ANIMATION(player.getObjectId(), castingSkill->getFirstTarget()->getObjectId(),
				castingSkill->getItemObjectId(), castingSkill->getItemTemplate()->getTemplateId(), 0, 3, 0),
			true);
	}

	if (Ptr<Player> attackingPlayer = runtime::as<Player>(lastAttacker); attackingPlayer && !lastAttacker->equals(getOwner())) {
		PacketSendUtility::sendPacket(*attackingPlayer, SM_SYSTEM_MESSAGE::STR_SKILL_TARGET_SKILL_CANCELED());
	}
}

void PlayerController::cancelUseItem() {
	getOwner().getObserveController()->abortItemUseObservers(); // each observer knows its item and cancels its own task, message and animation
}

void PlayerController::onDialogSelect(int32_t dialogActionId, int32_t prevDialogId, model::gameobjects::player::Player& player, int32_t questId,
	int32_t extendedRewardIndex) {
	switch (dialogActionId) {
		case model::DialogAction::BUY:
			PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_PRIVATE_STORE(getOwner().getStore(), player));
			break;
		case model::DialogAction::QUEST_ACCEPT_1:
		case model::DialogAction::QUEST_ACCEPT_SIMPLE:
			if (!getOwner().equals(player) && PositionUtil::isInRange(getOwner(), player, 100)) { // TODO check if owner really shared
				// Java: a NullPointerException for an unknown questId, which the client chooses (CM_DIALOG_SELECT checks it only for its own target)
				if (!detail::nonNull(dataholders::DataManager::QUEST_DATA->getQuestById(questId), "QUEST_DATA.getQuestById(questId)")
				       ->isCannotShare()) {
					Ref<questEngine::model::QuestEnv> env = questEngine::model::QuestEnv::create(nullptr, player, questId, dialogActionId);
					services::QuestService::startQuest(*env);
				}
			}
			break;
		default:
			break;
	}
}

void PlayerController::onLevelChange(int32_t oldLevel, int32_t newLevel) {
	if (oldLevel == newLevel)
		return;

	Player& player = getOwner();
	int32_t minNewLevel = oldLevel < newLevel ? oldLevel + 1 : oldLevel - 1; // for skill learning and other stuff that only wants the new level(s)

	if (configs::main::GSConfig::ENABLE_RATIO_LIMITATION &&
		(player.getAccount()->getNumberOf(player.getRace()) == 1 || player.getAccount()->getMaxPlayerLevel() == newLevel)) {
		if (oldLevel < configs::main::GSConfig::RATIO_MIN_REQUIRED_LEVEL && newLevel >= configs::main::GSConfig::RATIO_MIN_REQUIRED_LEVEL)
			GameServer::updateRatio(player.getRace(), 1);
		else if (oldLevel >= configs::main::GSConfig::RATIO_MIN_REQUIRED_LEVEL && newLevel < configs::main::GSConfig::RATIO_MIN_REQUIRED_LEVEL)
			GameServer::updateRatio(player.getRace(), -1);
	}

	player.getGameStats()->updateStatsTemplate();
	player.getCommonData()->updateMaxRepose();
	player.getCommonData()->resetSalvationPoints();
	upgradePlayer();
	PacketSendUtility::broadcastPacket(player,
		network::aion::serverpackets::SM_ACTION_ANIMATION(player.getObjectId(), model::animations::ActionAnimation::LEVEL_UP, newLevel), true);

	player.getNpcFactions().onLevelUp();
	questEngine::QuestEngine::getInstance().onLevelChanged(player);
	updateNearbyQuests();
	if (configs::main::HTMLConfig::ENABLE_GUIDES && player.isSpawned())
		services::HTMLService::sendGuideHtml(player, minNewLevel, newLevel);
	services::SkillLearnService::learnNewSkills(player, minNewLevel, newLevel);
	services::BonusPackService::getInstance().addPlayerCustomReward(player);
	services::FactionPackService::getInstance().addPlayerCustomReward(player);
	if (configs::main::CustomConfig::ENABLE_STARTER_KIT)
		services::reward::StarterKitService::getInstance().onLevelUp(player, minNewLevel, newLevel);
}

void PlayerController::upgradePlayer() {
	Player& player = getOwner();
	player.getLifeStats()->synchronizeWithMaxStats();
	player.getGameStats()->updateStatsVisually();

	if (player.isInTeam()) // SM_GROUP_MEMBER_INFO / SM_ALLIANCE_MEMBER_INFO task
		standins::teamStatUpdaterAdd(player);

	if (player.isLegionMember()) // SM_LEGION_UPDATE_MEMBER
		services::LegionService::getInstance().updateMemberInfo(player);
}

void PlayerController::onChangedPlayerAttributes() {
	Player& player = getOwner();
	player.clearKnownlist();
	sendPlayerInfoPackets(player);
	if (player.getSeeState() != 0)
		PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_PLAYER_STATE(player)); // needed to see hidden creatures again
	player.getEffectController()->updatePlayerEffectIcons(nullptr);
	player.updateKnownlist();
}

void PlayerController::startProtectionActiveTask() {
	Player& player = getOwner();
	if (!player.isProtectionActive()) {
		player.setVisualState(model::gameobjects::state::CreatureVisualState::BLINKING);
		attack::AttackUtil::cancelCastOn(player);
		attack::AttackUtil::removeTargetFrom(player);
		PacketSendUtility::broadcastToSightedPlayers(player, network::aion::serverpackets::SM_PLAYER_STATE(player), true);
		addTask(model::TaskId::PROTECTION_ACTIVE,
			utils::ThreadPoolManager::getInstance().schedule(this, &PlayerController::stopProtectionActiveTask, 60000));
	}
}

void PlayerController::stopProtectionActiveTask() {
	cancelTask(model::TaskId::PROTECTION_ACTIVE);
	Player& player = getOwner();
	if (player.isSpawned()) {
		player.unsetVisualState(model::gameobjects::state::CreatureVisualState::BLINKING);
		PacketSendUtility::broadcastToSightedPlayers(player, network::aion::serverpackets::SM_PLAYER_STATE(player), true);
		notifyAIOnMove();
	}
}

void PlayerController::onFlyTeleportEnd() {
	Player& player = getOwner();
	if (player.isUsingFlightPath(model::templates::flypath::FlightPath_Type::WINDSTREAM)) {
		player.unsetState(CreatureState::FLYING);
		player.unsetFlyState(FlyState::FLYING);
		player.setFlyState(FlyState::GLIDING);
		player.setState(CreatureState::ACTIVE);
		player.setState(CreatureState::GLIDING);
		player.getLifeStats()->triggerFpReduce();
		player.getGameStats()->updateStatsAndSpeedVisually();
	} else {
		player.unsetState(CreatureState::FLYING);
		if (configs::main::SecurityConfig::ENABLE_FLYPATH_VALIDATOR) {
			int64_t diff = (commons::utils::currentTimeMillis() - player.getFlyStartTime());
			const model::templates::flypath::FlyPathEntry* path = detail::nonNull(player.getCurrentFlyPath(), "player.getCurrentFlyPath()");

			if (player.getWorldId() != path->getEndWorldId()) {
				utils::audit::AuditLogger::log(player, "tried to use flyPath #" + std::to_string(path->getId()) + " from not native start world " +
					std::to_string(player.getWorldId()) + " (expected " + std::to_string(path->getEndWorldId()) + ")");
			}

			if (diff < path->getTimeInMs()) {
				utils::audit::AuditLogger::log(player,
					"ended fly path too early: Fly duration " + std::to_string(diff) + "ms instead of " + std::to_string(path->getTimeInMs()) + "ms");
				/*
				 * todo if works teleport player to start_* xyz, or even ban
				 */
			}

			player.setCurrentFlypath(nullptr);
		}
		player.setState(CreatureState::ACTIVE);
		updateZone();
	}
	player.setFlightPath(nullptr);
}

void PlayerController::startStance(int32_t skillId) {
	stopStance();
	stanceObserver = observer::StanceObserver::create(getOwner(), skillId);
	getOwner().getObserveController()->addObserver(*stanceObserver);
	PacketSendUtility::broadcastPacket(getOwner(), network::aion::serverpackets::SM_PLAYER_STANCE(getOwner(), 1), true);
}

void PlayerController::stopStance() {
	if (Ptr<observer::StanceObserver> observer = stanceObserver.get()) {
		getOwner().getObserveController()->removeObserver(*observer);
		getOwner().getEffectController()->removeEffect(observer->getStanceSkillId());
		PacketSendUtility::broadcastPacket(getOwner(), network::aion::serverpackets::SM_PLAYER_STANCE(getOwner(), 0), true);
		stanceObserver = nullptr;
	}
}

int32_t PlayerController::getStanceSkillId() {
	Ptr<observer::StanceObserver> observer = stanceObserver.get();
	return !observer ? 0 : observer->getStanceSkillId();
}

bool PlayerController::isUnderStance() {
	return static_cast<bool>(stanceObserver.get());
}

void PlayerController::updateSoulSickness(int32_t skillId) {
	Player& player = getOwner();
	Ptr<model::house::House> house = player.getActiveHouse();
	if (house)
		switch (house->getHouseType()) {
			case model::templates::housing::HouseType::MANSION:
			case model::templates::housing::HouseType::ESTATE:
			case model::templates::housing::HouseType::PALACE:
				return;
			default:
				break;
		}

	if (!player.hasPermission(configs::main::MembershipConfig::DISABLE_SOULSICKNESS)) {
		int32_t deathCount = player.getCommonData()->getDeathCount();
		if (deathCount < 10) {
			deathCount++;
			player.getCommonData()->setDeathCount(deathCount);
		}

		if (skillId == 0)
			skillId = 8291;
		skillengine::SkillEngine::getInstance().getSkill(player, skillId, deathCount, player)->useSkill();
	}
}

bool PlayerController::isInCombat() {
	return commons::utils::currentTimeMillis() - getLastCombatTime() <= 10000;
}

int64_t PlayerController::getLastCombatTime() {
	return std::max(lastAttackedMillis.get(), lastAttackMillis.get());
}

void PlayerController::enterCombat(bool attacking) {
	if (attacking)
		lastAttackMillis = commons::utils::currentTimeMillis();
	else
		lastAttackedMillis = commons::utils::currentTimeMillis();
	services::RecallService::getInstance().cancel(getOwner(), RecallService_CancelReason::CANCELLED);
}

void PlayerController::breakStanceObserver() {
	stanceObserver = nullptr;
}

} // namespace aion::gameserver::controllers
