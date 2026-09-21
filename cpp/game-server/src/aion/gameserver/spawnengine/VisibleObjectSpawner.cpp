#include "aion/gameserver/spawnengine/VisibleObjectSpawner.h"

#include <cmath>
#include <exception>
#include <memory>
#include <optional>
#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/ai/event/AIEventType.h"
#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/configs/main/SiegeConfig.h"
#include "aion/gameserver/controllers/GatherableController.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/PetController.h"
#include "aion/gameserver/controllers/SiegeWeaponController.h"
#include "aion/gameserver/controllers/SummonController.h"
#include "aion/gameserver/controllers/TrapController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/dataholders/PetData.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/geoEngine/collision/CollisionIntention.h"
#include "aion/gameserver/geoEngine/collision/CollisionIntentionInfo.h"
#include "aion/gameserver/geoEngine/collision/IgnoreProperties.h"
#include "aion/gameserver/geoEngine/math/Vector3f.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/gameobjects/Gatherable.h"
#include "aion/gameserver/model/gameobjects/GroupGate.h"
#include "aion/gameserver/model/gameobjects/Homing.h"
#include "aion/gameserver/model/gameobjects/Kisk.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/Pet.h"
#include "aion/gameserver/model/gameobjects/Servant.h"
#include "aion/gameserver/model/gameobjects/Summon.h"
#include "aion/gameserver/model/gameobjects/SummonedHouseNpc.h"
#include "aion/gameserver/model/gameobjects/Trap.h"
#include "aion/gameserver/model/gameobjects/player/PetCommonData.h"
#include "aion/gameserver/model/gameobjects/player/PetList.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/siege/SiegeNpc.h"
#include "aion/gameserver/model/gameobjects/state/CreatureState.h"
#include "aion/gameserver/model/rift/RiftLocation.h"
#include "aion/gameserver/model/skill/NpcSkillEntry.h"
#include "aion/gameserver/model/skill/NpcSkillList.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/model/templates/spawns/riftspawns/RiftSpawnTemplate.h"
#include "aion/gameserver/model/templates/spawns/siegespawns/SiegeSpawnTemplate.h"
#include "aion/gameserver/model/templates/spawns/vortexspawns/VortexSpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PLAYER_STATE.h"
#include "aion/gameserver/services/RiftService.h"
#include "aion/gameserver/skillengine/condition/HpCondition.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/spawnengine/SpawnEngine.h"
#include "aion/gameserver/spawnengine/WalkerFormator.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"
#include "aion/gameserver/world/geo/GeoService.h"
#include "aion/gameserver/world/knownlist/CreatureAwareKnownList.h"
#include "aion/gameserver/world/knownlist/FlagKnownList.h"
#include "aion/gameserver/world/knownlist/NpcKnownList.h"
#include "aion/gameserver/world/knownlist/PlayerAwareKnownList.h"
#include "aion/commons/utils/WindowsMacroGuard.h"

namespace aion::gameserver::spawnengine {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.spawnengine.VisibleObjectSpawner");

namespace {

using model::gameobjects::Npc;
using model::gameobjects::VisibleObject;

/** Java: Math.toRadians(angdeg) = angdeg * DEGREES_TO_RADIANS */
double toRadians(double angdeg) {
	constexpr double DEGREES_TO_RADIANS = 0.017453292519943295;
	return angdeg * DEGREES_TO_RADIANS;
}

const model::templates::npc::NpcTemplate* npcTemplateOf(int32_t npcId) {
	return dataholders::DataManager::NPC_DATA->getNpcTemplate(npcId);
}

} // namespace

runtime::Ref<VisibleObject> VisibleObjectSpawner::spawnNpc(model::templates::spawns::SpawnTemplate& spawn, int32_t instanceIndex) {
	int32_t npcId = spawn.getNpcId();
	const model::templates::npc::NpcTemplate* npcTemplate = npcTemplateOf(npcId);
	if (npcTemplate == nullptr) {
		log.error("No template for NPC {}", npcId);
		return nullptr;
	}
	runtime::Ref<Npc> npc = VisibleObject::create<Npc>(std::make_unique<controllers::NpcController>(), spawn, npcTemplate);
	npc->setCreatorId(spawn.getCreatorId());
	if (npc->isFlag())
		npc->setKnownlist(std::make_unique<world::knownlist::FlagKnownList>(*npc));
	else
		npc->setKnownlist(std::make_unique<world::knownlist::NpcKnownList>(*npc));
	npc->setEffectController(std::make_unique<controllers::effect::EffectController>(*npc));

	if (WalkerFormator::processClusteredNpc(*npc, spawn.getWorldId(), instanceIndex))
		return npc;

	try {
		SpawnEngine::bringIntoWorld(*npc, spawn, instanceIndex);
	} catch (const std::exception& ex) {
		log.error("Error during spawn:", ex);
		npc->getController().delete_();
	}
	return npc;
}

runtime::Ref<model::gameobjects::SummonedHouseNpc> VisibleObjectSpawner::spawnHouseNpc(model::templates::spawns::SpawnTemplate& spawn,
	int32_t instanceIndex, model::house::House& creator) {
	runtime::Ref<model::gameobjects::SummonedHouseNpc> npc =
		VisibleObject::create<model::gameobjects::SummonedHouseNpc>(std::make_unique<controllers::NpcController>(), spawn, creator);
	SpawnEngine::bringIntoWorld(*npc, spawn, instanceIndex);
	return npc;
}

runtime::Ref<VisibleObject> VisibleObjectSpawner::spawnRiftNpc(model::templates::spawns::riftspawns::RiftSpawnTemplate& spawn, int32_t instanceIndex) {
	if (!configs::main::CustomConfig::RIFT_ENABLED.load()) {
		return nullptr;
	}

	int32_t npcId = spawn.getNpcId();
	const model::templates::npc::NpcTemplate* npcTemplate = npcTemplateOf(npcId);
	if (npcTemplate == nullptr) {
		log.error("No template for NPC {}", npcId);
		return nullptr;
	}
	runtime::Ref<Npc> npc;

	int32_t spawnId = spawn.getId();
	runtime::Ptr<model::rift::RiftLocation> loc = services::RiftService::getInstance().getRiftLocation(spawnId);
	if (loc->isOpened() && spawnId == loc->getId()) {
		npc = VisibleObject::create<Npc>(std::make_unique<controllers::NpcController>(), spawn, npcTemplate);
		npc->setKnownlist(std::make_unique<world::knownlist::NpcKnownList>(*npc));
	} else {
		return nullptr;
	}
	npc->setEffectController(std::make_unique<controllers::effect::EffectController>(*npc));
	SpawnEngine::bringIntoWorld(*npc, spawn, instanceIndex);
	return npc;
}

runtime::Ref<VisibleObject> VisibleObjectSpawner::spawnSiegeNpc(model::templates::spawns::siegespawns::SiegeSpawnTemplate& spawn,
	int32_t instanceIndex) {
	if (!configs::main::SiegeConfig::SIEGE_ENABLED.load())
		return nullptr;

	const model::templates::npc::NpcTemplate* npcTemplate = npcTemplateOf(spawn.getNpcId());
	if (npcTemplate == nullptr) {
		log.error("No template for NPC {}", spawn.getNpcId());
		return nullptr;
	}
	runtime::Ref<Npc> npc = VisibleObject::create<model::gameobjects::siege::SiegeNpc>(std::make_unique<controllers::NpcController>(), spawn, npcTemplate);
	npc->setKnownlist(std::make_unique<world::knownlist::NpcKnownList>(*npc));
	npc->setEffectController(std::make_unique<controllers::effect::EffectController>(*npc));
	SpawnEngine::bringIntoWorld(*npc, spawn, instanceIndex);
	return npc;
}

runtime::Ref<VisibleObject> VisibleObjectSpawner::spawnInvasionNpc(model::templates::spawns::vortexspawns::VortexSpawnTemplate& spawn,
	int32_t instanceIndex) {
	if (!configs::main::CustomConfig::VORTEX_ENABLED.load()) {
		return nullptr;
	}

	const model::templates::npc::NpcTemplate* npcTemplate = npcTemplateOf(spawn.getNpcId());
	if (npcTemplate == nullptr) {
		log.error("No template for NPC {}", spawn.getNpcId());
		return nullptr;
	}
	runtime::Ref<Npc> npc = VisibleObject::create<Npc>(std::make_unique<controllers::NpcController>(), spawn, npcTemplate);
	npc->setKnownlist(std::make_unique<world::knownlist::NpcKnownList>(*npc));
	npc->setEffectController(std::make_unique<controllers::effect::EffectController>(*npc));
	SpawnEngine::bringIntoWorld(*npc, spawn, instanceIndex);
	return npc;
}

runtime::Ref<model::gameobjects::Gatherable> VisibleObjectSpawner::spawnGatherable(model::templates::spawns::SpawnTemplate& spawn,
	int32_t instanceIndex) {
	runtime::Ref<model::gameobjects::Gatherable> gatherable =
		model::gameobjects::VisibleObject::create<model::gameobjects::Gatherable>(spawn, std::make_unique<controllers::GatherableController>());
	SpawnEngine::bringIntoWorld(*gatherable, spawn, instanceIndex);
	return gatherable;
}

runtime::Ref<model::gameobjects::Trap> VisibleObjectSpawner::spawnTrap(model::templates::spawns::SpawnTemplate& spawn, int32_t instanceIndex,
	model::gameobjects::Creature& creator) {
	runtime::Ref<model::gameobjects::Trap> trap = VisibleObject::create<model::gameobjects::Trap>(std::make_unique<controllers::TrapController>(), spawn, creator);
	SpawnEngine::bringIntoWorld(*trap, spawn, instanceIndex);
	utils::PacketSendUtility::broadcastPacket(*trap, network::aion::serverpackets::SM_PLAYER_STATE(*trap));
	return trap;
}

runtime::Ref<model::gameobjects::GroupGate> VisibleObjectSpawner::spawnGroupGate(model::templates::spawns::SpawnTemplate& spawn, int32_t instanceIndex,
	model::gameobjects::Creature& creator) {
	runtime::Ref<model::gameobjects::GroupGate> groupgate =
		VisibleObject::create<model::gameobjects::GroupGate>(std::make_unique<controllers::NpcController>(), spawn, creator);
	SpawnEngine::bringIntoWorld(*groupgate, spawn, instanceIndex);
	return groupgate;
}

runtime::Ref<model::gameobjects::Kisk> VisibleObjectSpawner::spawnKisk(model::templates::spawns::SpawnTemplate& spawn, int32_t instanceIndex,
	model::gameobjects::player::Player& creator) {
	runtime::Ref<model::gameobjects::Kisk> kisk = VisibleObject::create<model::gameobjects::Kisk>(std::make_unique<controllers::NpcController>(), spawn, creator);
	SpawnEngine::bringIntoWorld(*kisk, spawn, instanceIndex);
	return kisk;
}

runtime::Ref<Npc> VisibleObjectSpawner::spawnPostman(model::gameobjects::player::Player& owner) {
	int32_t npcId = owner.getRace() == model::Race::ELYOS ? 798100 : 798101;
	const model::templates::npc::NpcTemplate* template_ = npcTemplateOf(npcId);
	double radian = toRadians(utils::PositionUtil::convertHeadingToAngle(owner.getHeading()));
	geoEngine::math::Vector3f pos = world::geo::GeoService::getInstance().getClosestCollision(owner,
		owner.getX() + static_cast<float>(std::cos(radian) * 7), owner.getY() + static_cast<float>(std::sin(radian) * 7), owner.getZ());
	runtime::Ref<model::templates::spawns::SpawnTemplate> spawn =
		SpawnEngine::newSingleTimeSpawn(owner.getWorldId(), npcId, pos.getX(), pos.getY(), pos.getZ(), int8_t{0});
	runtime::Ref<Npc> postman = VisibleObject::create<Npc>(std::make_unique<controllers::NpcController>(), *spawn, template_);
	postman->setCreatorId(owner.getObjectId());
	postman->setMasterName(owner.getName());
	postman->setKnownlist(std::make_unique<world::knownlist::PlayerAwareKnownList>(*postman));
	postman->setEffectController(std::make_unique<controllers::effect::EffectController>(*postman));
	SpawnEngine::bringIntoWorld(*postman, *spawn, owner.getInstanceId());
	owner.setPostman(postman);
	return postman;
}

runtime::Ref<Npc> VisibleObjectSpawner::spawnFunctionalNpc(model::gameobjects::player::Player& owner, int32_t npcId,
	skillengine::effect::SummonOwner summonOwner) {
	const model::templates::npc::NpcTemplate* template_ = npcTemplateOf(npcId);
	double radian = toRadians(utils::PositionUtil::convertHeadingToAngle(owner.getHeading()));
	geoEngine::math::Vector3f pos = world::geo::GeoService::getInstance().getClosestCollision(owner,
		owner.getX() + static_cast<float>(std::cos(radian) * 1), owner.getY() + static_cast<float>(std::sin(radian) * 1), owner.getZ());
	int8_t heading = utils::PositionUtil::getHeadingTowards(pos.getX(), pos.getY(), owner.getX(), owner.getY());
	runtime::Ref<model::templates::spawns::SpawnTemplate> spawn =
		SpawnEngine::newSingleTimeSpawn(owner.getWorldId(), npcId, pos.getX(), pos.getY(), pos.getZ(), heading);
	runtime::Ref<Npc> functionalNpc = VisibleObject::create<Npc>(std::make_unique<controllers::NpcController>(), *spawn, template_);
	functionalNpc->setKnownlist(std::make_unique<world::knownlist::PlayerAwareKnownList>(*functionalNpc));
	functionalNpc->setEffectController(std::make_unique<controllers::effect::EffectController>(*functionalNpc));
	functionalNpc->setCreatorId(owner.getObjectId());
	functionalNpc->setSummonOwner(summonOwner);
	SpawnEngine::bringIntoWorld(*functionalNpc, *spawn, owner.getInstanceId());
	return functionalNpc;
}

runtime::Ref<model::gameobjects::Servant> VisibleObjectSpawner::spawnServant(model::templates::spawns::SpawnTemplate& spawn, int32_t instanceIndex,
	model::gameobjects::Creature& creator, int32_t level, model::gameobjects::NpcObjectType objectType) {
	// Java passes creator.getLevel(), not the level argument
	runtime::Ref<model::gameobjects::Servant> servant = VisibleObject::create<model::gameobjects::Servant>(
		std::make_unique<controllers::NpcController>(), spawn, creator.getLevel(), creator);
	servant->setNpcObjectType(objectType);
	servant->setUpStats();
	SpawnEngine::bringIntoWorld(*servant, spawn, instanceIndex);
	if (runtime::Ptr<model::skill::NpcSkillList> skillList = servant->getSkillList()) {
		runtime::Ptr<model::skill::NpcSkillEntry> skill = skillList->getRandomSkill();
		if (skill) {
			const skillengine::model::SkillTemplate* st = dataholders::DataManager::SKILL_DATA->getSkillTemplate(skill->getSkillId());
			if (st->getStartconditions() != nullptr && st->getHpCondition() != nullptr) {
				int32_t hp = (st->getHpCondition()->getHpValue() * 3);
				servant->getLifeStats()->setCurrentHp(hp);
			}
		}
	}
	return servant;
}

runtime::Ref<model::gameobjects::Servant> VisibleObjectSpawner::spawnEnemyServant(model::templates::spawns::SpawnTemplate& spawn,
	int32_t instanceIndex, model::gameobjects::Creature& creator, int8_t servantLvl) {
	runtime::Ref<model::gameobjects::Servant> servant =
		VisibleObject::create<model::gameobjects::Servant>(std::make_unique<controllers::NpcController>(), spawn, servantLvl, creator);
	servant->setNpcObjectType(model::gameobjects::NpcObjectType::SERVANT);
	SpawnEngine::bringIntoWorld(*servant, spawn, instanceIndex);
	return servant;
}

runtime::Ref<model::gameobjects::Homing> VisibleObjectSpawner::spawnHoming(model::templates::spawns::SpawnTemplate& spawn, int32_t instanceIndex,
	model::gameobjects::Creature& creator, int32_t attackCount, int32_t skillId) {
	runtime::Ref<model::gameobjects::Homing> homing = VisibleObject::create<model::gameobjects::Homing>(
		std::make_unique<controllers::NpcController>(), spawn, creator.getLevel(), creator, skillId);
	homing->setState(model::gameobjects::state::CreatureState::WEAPON_EQUIPPED);
	homing->setAttackCount(attackCount);
	SpawnEngine::bringIntoWorld(*homing, spawn, instanceIndex);
	return homing;
}

runtime::Ref<model::gameobjects::Summon> VisibleObjectSpawner::spawnSummon(model::gameobjects::player::Player& creator, int32_t npcId,
	int32_t skillId, int32_t time) {
	double radian = toRadians(utils::PositionUtil::convertHeadingToAngle(creator.getHeading()));
	float x = creator.getX() + static_cast<float>(std::cos(radian) * 2);
	float y = creator.getY() + static_cast<float>(std::sin(radian) * 2);
	geoEngine::math::Vector3f pos = world::geo::GeoService::getInstance().getClosestCollision(creator, x, y, creator.getZ(), true,
		getId(geoEngine::collision::CollisionIntention::DEFAULT_COLLISIONS), *geoEngine::collision::IgnoreProperties::of(creator.getRace()));
	int8_t heading = creator.getHeading();
	int32_t worldId = creator.getWorldId();
	int32_t instanceId = creator.getInstanceId();

	runtime::Ref<model::templates::spawns::SpawnTemplate> spawn = SpawnEngine::newSingleTimeSpawn(worldId, npcId, pos.getX(), pos.getY(), pos.getZ(), heading);
	const model::templates::npc::NpcTemplate* npcTemplate = npcTemplateOf(npcId);

	bool isSiegeWeapon = npcTemplate->getAiName() == std::optional<std::string>("siege_weapon");
	std::unique_ptr<controllers::SummonController> controller;
	if (isSiegeWeapon)
		controller = std::make_unique<controllers::SiegeWeaponController>(npcId);
	else
		controller = std::make_unique<controllers::SummonController>();
	runtime::Ref<model::gameobjects::Summon> summon = VisibleObject::create<model::gameobjects::Summon>(
		utils::idfactory::IDFactory::getInstance().nextId(), std::move(controller), *spawn, npcTemplate, creator, time);
	summon->setKnownlist(std::make_unique<world::knownlist::CreatureAwareKnownList>(*summon));
	summon->setEffectController(std::make_unique<controllers::effect::EffectController>(*summon));
	summon->getLifeStats()->synchronizeWithMaxStats();
	summon->setSummonedBySkillId(skillId);

	SpawnEngine::bringIntoWorld(*summon, *spawn, instanceId);
	if (isSiegeWeapon)
		summon->getAi().onGeneralEvent(ai::event::AIEventType::SPAWNED);
	return summon;
}

runtime::Ref<model::gameobjects::Pet> VisibleObjectSpawner::spawnPet(model::gameobjects::player::Player& player, int32_t templateId) {
	runtime::Ptr<model::gameobjects::player::PetCommonData> petCommonData = player.getPetList().getPet(templateId);
	if (!petCommonData)
		return nullptr;

	const model::templates::pet::PetTemplate* petTemplate = dataholders::DataManager::PET_DATA->getPetTemplate(templateId);
	if (petTemplate == nullptr)
		return nullptr;

	runtime::Ref<model::gameobjects::Pet> pet =
		VisibleObject::create<model::gameobjects::Pet>(petTemplate, std::make_unique<controllers::PetController>(), *petCommonData, player);
	pet->setKnownlist(std::make_unique<world::knownlist::PlayerAwareKnownList>(*pet));

	float x = player.getX();
	float y = player.getY();
	float z = player.getZ();
	int8_t heading = player.getHeading();
	int32_t worldId = player.getWorldId();
	int32_t instanceId = player.getInstanceId();
	runtime::Ref<model::templates::spawns::SpawnTemplate> spawn = SpawnEngine::newSingleTimeSpawn(worldId, templateId, x, y, z, heading);
	SpawnEngine::bringIntoWorld(*pet, *spawn, instanceId);
	player.setPet(pet);
	return pet;
}

} // namespace aion::gameserver::spawnengine
