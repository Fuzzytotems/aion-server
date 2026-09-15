#include "aion/gameserver/model/gameobjects/Npc.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <string_view>

#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/movement/NpcMoveController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/TribeRelationsData.h"
#include "aion/gameserver/dataholders/loadingutils/adapters/NpcEquipmentList.h"
#include "aion/gameserver/model/CreatureType.h"
#include "aion/gameserver/model/CreatureTypeInfo.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/TribeClass.h"
#include "aion/gameserver/model/gameobjects/TransformModel.h"
#include "aion/gameserver/model/gameobjects/player/CustomPlayerState.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/state/CreatureSeeState.h"
#include "aion/gameserver/model/gameobjects/state/CreatureSeeStateInfo.h"
#include "aion/gameserver/model/items/NpcEquippedGear.h"
#include "aion/gameserver/model/skill/NpcSkillEntry.h"
#include "aion/gameserver/model/skill/NpcSkillList.h"
#include "aion/gameserver/model/stats/container/NpcGameStats.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/model/templates/item/ItemAttackType.h"
#include "aion/gameserver/model/templates/npc/GroupDropType.h"
#include "aion/gameserver/model/templates/npc/NpcRank.h"
#include "aion/gameserver/model/templates/npc/AbyssNpcType.h"
#include "aion/gameserver/model/templates/npc/NpcRating.h"
#include "aion/gameserver/model/templates/npc/NpcRatingInfo.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/npc/NpcTemplateType.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CUSTOM_SETTINGS.h"
#include "aion/gameserver/runtime/sync/Monitor.h"
#include "aion/gameserver/services/TribeRelationService.h"
#include "aion/gameserver/spawnengine/WalkerGroup.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::model::gameobjects {

namespace {

/** Java reads the nullable template attribute directly; every npc_template of the static data has it, a missing one is Java's NullPointerException */
template <class T>
T requireTemplateValue(const std::optional<T>& value, std::string_view attribute) {
	if (!value)
		throw runtime::NullPointerException("NpcTemplate." + std::string(attribute));
	return *value;
}

} // namespace

Npc::Npc(CreateKey key, std::unique_ptr<controllers::NpcController> controller, templates::spawns::SpawnTemplate& spawnTemplate,
	const templates::npc::NpcTemplate* objectTemplate)
	: Creature(key, utils::idfactory::IDFactory::getInstance().nextId(), std::move(controller), spawnTemplate,
		  // Java: Objects.requireNonNull(objectTemplate)
		  objectTemplate != nullptr ? objectTemplate : throw runtime::NullPointerException("objectTemplate"),
		  world::WorldPosition::create(spawnTemplate.getWorldId()), true),
	  skillList(std::make_unique<skill::NpcSkillList>(*this)) {
}

Npc::~Npc() = default;

void Npc::postConstruct() {
	Creature::postConstruct();
	getController().setOwner(*this);
	moveController.set(std::make_unique<controllers::movement::NpcMoveController>(*this));
	setupStatContainers();
}

runtime::Ptr<controllers::movement::NpcMoveController> Npc::getMoveController() const {
	return runtime::cast<controllers::movement::NpcMoveController>(Creature::getMoveController());
}

void Npc::setupStatContainers() {
	setGameStats(std::make_unique<stats::container::NpcGameStats>(*this));
	setLifeStats(std::make_unique<stats::container::NpcLifeStats>(*this));
}

runtime::Ptr<stats::container::NpcLifeStats> Npc::getLifeStats() const {
	return runtime::cast<stats::container::NpcLifeStats>(Creature::getLifeStats());
}

runtime::Ptr<stats::container::NpcGameStats> Npc::getGameStats() const {
	return runtime::cast<stats::container::NpcGameStats>(Creature::getGameStats());
}

runtime::Ptr<skill::NpcSkillList> Npc::getSkillList() const {
	return runtime::Ptr<skill::NpcSkillList>(skillList.get());
}

void Npc::setWalkerGroup(runtime::Ptr<spawnengine::WalkerGroup> wg) {
	walkerGroup.set(wg);
}

controllers::NpcController& Npc::getController() const {
	return static_cast<controllers::NpcController&>(Creature::getController());
}

const templates::npc::NpcTemplate* Npc::getObjectTemplate() const {
	return static_cast<const templates::npc::NpcTemplate*>(Creature::getObjectTemplate());
}

std::optional<std::string> Npc::getMasterName() {
	const std::string& name = masterName.get();
	if (name.empty())
		return std::nullopt;
	return name;
}

void Npc::setMasterName(std::string_view value) {
	masterName.set(std::string(value));
}

int32_t Npc::getCreatorId() {
	return creatorId.get();
}

int32_t Npc::getNpcId() {
	return getObjectTemplate()->getTemplateId();
}

int8_t Npc::getLevel() {
	return getObjectTemplate()->getLevel();
}

templates::npc::AbyssNpcType Npc::getAbyssNpcType() {
	return getObjectTemplate()->getAbyssNpcType();
}

templates::npc::NpcRating Npc::getRating() {
	return requireTemplateValue(getObjectTemplate()->getRating(), "rating");
}

templates::npc::NpcRank Npc::getRank() {
	return requireTemplateValue(getObjectTemplate()->getRank(), "rank");
}

templates::npc::NpcTemplateType Npc::getNpcTemplateType() {
	return getObjectTemplate()->getNpcTemplateType();
}

int32_t Npc::getHpGauge() {
	return getObjectTemplate()->getHpGauge();
}

templates::item::ItemAttackType Npc::getAttackType() {
	return getAi().modifyAttackType(templates::item::ItemAttackType::PHYSICAL);
}

runtime::Ptr<skill::NpcSkillEntry> Npc::getNextQueuedSkill() {
	SYNCHRONIZED(queuedSkills) {
		return queuedSkills.peek();
	}
}

bool Npc::hasQueuedSkill(const std::function<bool(skill::NpcSkillEntry&)>& filter) {
	SYNCHRONIZED(queuedSkills) {
		for (runtime::Ptr<skill::NpcSkillEntry> entry : queuedSkills) {
			if (filter(*entry))
				return true;
		}
		return false;
	}
}

void Npc::removeNextQueuedSkill(skill::NpcSkillEntry& skill) {
	SYNCHRONIZED(queuedSkills) {
		if (queuedSkills.peek() == &skill) {
			queuedSkills.poll();
		}
	}
}

void Npc::clearQueuedSkills() {
	SYNCHRONIZED(queuedSkills) {
		queuedSkills.clear();
	}
}

void Npc::queueSkill(skill::NpcSkillEntry& skill) {
	SYNCHRONIZED(queuedSkills) {
		queuedSkills.offer(runtime::Ref<skill::NpcSkillEntry>(skill));
	}
}

void Npc::queueSkill(int32_t skillId, int32_t level) {
	AION_UNPORTED();
}

void Npc::queueSkill(int32_t skillId, int32_t level, int32_t nextSkillTime) {
	AION_UNPORTED();
}

void Npc::queueSkill(int32_t skillId, int32_t level, int32_t nextSkillTime, templates::npcskill::NpcSkillTargetAttribute npcSkillTargetAttribute) {
	AION_UNPORTED();
}

bool Npc::isWalker() {
	return isRandomWalker() || isPathWalker();
}

bool Npc::isRandomWalker() {
	return getSpawn()->getRandomWalkRange() > 0;
}

bool Npc::isPathWalker() {
	return getSpawn()->getWalkerId().has_value();
}

std::optional<TribeClass> Npc::getTribe() {
	if (runtime::Ptr<player::Player> player = runtime::as<player::Player>(getCreator()))
		return player->getTribe();
	std::optional<TribeClass> transformTribe = isTransformed() ? getTransformModel().getTribe() : std::nullopt;
	if (transformTribe) {
		return transformTribe;
	}
	return getObjectTemplate()->getTribe();
}

TribeClass Npc::getBaseTribe() {
	std::optional<TribeClass> tribe = getTribe();
	if (!tribe) // Java: tribeNameMap.get(null) is null, then tribe.getBase() throws
		throw runtime::NullPointerException("Tribe of npc " + std::to_string(getNpcId()) + " is null");
	return dataholders::DataManager::TRIBE_RELATIONS_DATA->getBaseTribe(*tribe);
}

int32_t Npc::getAggroRange() {
	return getAi().modifyAggroRange(getObjectTemplate()->getAggroRange());
}

int32_t Npc::getShortAggroRange() {
	int32_t aggroRange = getAggroRange();
	return aggroRange < 8 ? aggroRange / 2 : 4;
}

int32_t Npc::getAggroAngle() {
	return getAi().modifyAggroAngle(getObjectTemplate()->getAggroAngle());
}

bool Npc::isAtSpawnLocation() {
	runtime::Ptr<templates::spawns::SpawnTemplate> spawn = getSpawn();
	return utils::PositionUtil::isInRange(*this, spawn->getX(), spawn->getY(), spawn->getZ(), 1);
}

bool Npc::isEnemy(Creature& creature) {
	return creature.isEnemyFrom(*this) || this->isEnemyFrom(creature);
}

bool Npc::isEnemyFrom(Creature& creature) {
	return services::TribeRelationService::isAggressive(creature, *this) || services::TribeRelationService::isHostile(creature, *this);
}

bool Npc::isEnemyFrom(Npc& npc) {
	return services::TribeRelationService::isAggressive(*this, npc) || services::TribeRelationService::isHostile(*this, npc);
}

bool Npc::isEnemyFrom(player::Player& player) {
	return player.isEnemyFrom(*this);
}

CreatureType Npc::getType(Creature& creature) {
	std::optional<CreatureType> overridden = overriddenType.get();
	CreatureType type = overridden ? *overridden : getRelationBasedType(creature);
	if (player::Player* player = dynamic_cast<player::Player*>(&creature)) {
		if (player->isInCustomState(player::CustomPlayerState::ENEMY_OF_ALL_NPCS) && type != CreatureType::ATTACKABLE && type != CreatureType::AGGRESSIVE)
			return CreatureType::ATTACKABLE;
		if (player->isInCustomState(player::CustomPlayerState::NEUTRAL_TO_ALL_NPCS) && (type == CreatureType::ATTACKABLE || type == CreatureType::AGGRESSIVE))
			return CreatureType::PEACE;
	}
	return type;
}

CreatureType Npc::getRelationBasedType(Creature& creature) {
	if (services::TribeRelationService::isNone(*this, creature))
		return CreatureType::PEACE;
	else if (services::TribeRelationService::isAggressive(*this, creature))
		return CreatureType::AGGRESSIVE;
	else if (services::TribeRelationService::isHostile(*this, creature))
		return CreatureType::ATTACKABLE;
	else if (services::TribeRelationService::isFriend(*this, creature) || services::TribeRelationService::isNeutral(*this, creature))
		return CreatureType::FRIEND;
	else if (services::TribeRelationService::isSupport(*this, creature))
		return CreatureType::SUPPORT;
	return CreatureType::ATTACKABLE;
}

void Npc::overrideNpcType(std::optional<CreatureType> newType) {
	overriddenType.set(newType);
	if (isSpawned()) {
		std::optional<CreatureType> current = overriddenType.get();
		if (current)
			utils::PacketSendUtility::broadcastPacket(*this, network::aion::serverpackets::SM_CUSTOM_SETTINGS(getObjectId(), 0, getId(*current), 0));
		else
			getKnownList().forEachPlayer([this](player::Player& p) {
				utils::PacketSendUtility::sendPacket(p, network::aion::serverpackets::SM_CUSTOM_SETTINGS(getObjectId(), 0, getId(getType(p)), 0));
			});
	}
}

double Npc::getDistanceToSpawnLocation() {
	runtime::Ptr<templates::spawns::SpawnTemplate> spawn = getSpawn();
	return utils::PositionUtil::getDistance(spawn->getX(), spawn->getY(), spawn->getZ(), getX(), getY(), getZ());
}

int32_t Npc::getSeeState() {
	int32_t skillSeeState = Creature::getSeeState();
	int32_t congenitalSeeState = state::getId(templates::npc::getCongenitalSeeState(requireTemplateValue(getObjectTemplate()->getRating(), "rating")));
	return std::max(skillSeeState, congenitalSeeState);
}

runtime::Ptr<VisibleObject> Npc::getCreator() {
	int32_t id = creatorId.get();
	return id == 0 ? nullptr : world::World::getInstance().findVisibleObject(id);
}

bool Npc::isFlag() {
	return getObjectTemplate()->getNpcTemplateType() == templates::npc::NpcTemplateType::FLAG;
}

bool Npc::isRaidMonster() {
	return getObjectTemplate()->getNpcTemplateType() == templates::npc::NpcTemplateType::RAID_MONSTER;
}

int32_t Npc::getCancelLevel() {
	return getObjectTemplate()->getCancelLevel();
}

bool Npc::isBoss() {
	return getObjectTemplate()->getRating() == templates::npc::NpcRating::HERO || getObjectTemplate()->getRating() == templates::npc::NpcRating::LEGENDARY;
}

bool Npc::hasStatic() {
	return getSpawn()->getStaticId() != 0;
}

Race Npc::getRace() {
	return getObjectTemplate()->getRace();
}

bool Npc::canSell() {
	AION_UNPORTED();
}

bool Npc::canBuy() {
	return getObjectTemplate()->supportsAction(DialogAction::SELL) || canSell();
}

bool Npc::canTradeIn() {
	AION_UNPORTED();
}

bool Npc::canPurchase() {
	AION_UNPORTED();
}

templates::npc::GroupDropType Npc::getGroupDrop() {
	return requireTemplateValue(getObjectTemplate()->getGroupDrop(), "groupDrop");
}

void Npc::overrideEquipmentList(std::unique_ptr<dataholders::loadingutils::adapters::NpcEquipmentList> v) {
	overriddenEquipment.set(items::NpcEquippedGear::create(std::move(v)));
}

runtime::Ptr<items::NpcEquippedGear> Npc::getOverrideEquipment() {
	runtime::Ptr<items::NpcEquippedGear> overridden = overriddenEquipment.get();
	if (overridden)
		return overridden;
	return getObjectTemplate()->getEquipment();
}

} // namespace aion::gameserver::model::gameobjects
