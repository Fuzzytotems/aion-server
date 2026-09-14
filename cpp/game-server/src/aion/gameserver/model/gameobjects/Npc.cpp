#include "aion/gameserver/model/gameobjects/Npc.h"

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/movement/NpcMoveController.h"
#include "aion/gameserver/model/items/NpcEquippedGear.h"
#include "aion/gameserver/model/skill/NpcSkillEntry.h"
#include "aion/gameserver/model/skill/NpcSkillList.h"
#include "aion/gameserver/model/stats/container/NpcGameStats.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/spawnengine/WalkerGroup.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::model::gameobjects {

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
	AION_UNPORTED();
}

templates::npc::AbyssNpcType Npc::getAbyssNpcType() {
	AION_UNPORTED();
}

templates::npc::NpcRating Npc::getRating() {
	AION_UNPORTED();
}

templates::npc::NpcRank Npc::getRank() {
	AION_UNPORTED();
}

templates::npc::NpcTemplateType Npc::getNpcTemplateType() {
	AION_UNPORTED();
}

int32_t Npc::getHpGauge() {
	AION_UNPORTED();
}

templates::item::ItemAttackType Npc::getAttackType() {
	AION_UNPORTED();
}

runtime::Ptr<skill::NpcSkillEntry> Npc::getNextQueuedSkill() {
	AION_UNPORTED();
}

bool Npc::hasQueuedSkill(const std::function<bool(skill::NpcSkillEntry&)>& filter) {
	AION_UNPORTED();
}

void Npc::removeNextQueuedSkill(skill::NpcSkillEntry& skill) {
	AION_UNPORTED();
}

void Npc::clearQueuedSkills() {
	AION_UNPORTED();
}

void Npc::queueSkill(skill::NpcSkillEntry& skill) {
	AION_UNPORTED();
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
	AION_UNPORTED();
}

bool Npc::isRandomWalker() {
	AION_UNPORTED();
}

bool Npc::isPathWalker() {
	AION_UNPORTED();
}

std::optional<TribeClass> Npc::getTribe() {
	AION_UNPORTED();
}

TribeClass Npc::getBaseTribe() {
	AION_UNPORTED();
}

int32_t Npc::getAggroRange() {
	AION_UNPORTED();
}

int32_t Npc::getShortAggroRange() {
	AION_UNPORTED();
}

int32_t Npc::getAggroAngle() {
	AION_UNPORTED();
}

bool Npc::isAtSpawnLocation() {
	AION_UNPORTED();
}

bool Npc::isEnemy(Creature& creature) {
	AION_UNPORTED();
}

bool Npc::isEnemyFrom(Creature& creature) {
	AION_UNPORTED();
}

bool Npc::isEnemyFrom(Npc& npc) {
	AION_UNPORTED();
}

bool Npc::isEnemyFrom(player::Player& player) {
	AION_UNPORTED();
}

CreatureType Npc::getType(Creature& creature) {
	AION_UNPORTED();
}

CreatureType Npc::getRelationBasedType(Creature& creature) {
	AION_UNPORTED();
}

void Npc::overrideNpcType(std::optional<CreatureType> newType) {
	AION_UNPORTED();
}

double Npc::getDistanceToSpawnLocation() {
	AION_UNPORTED();
}

int32_t Npc::getSeeState() {
	AION_UNPORTED();
}

runtime::Ptr<VisibleObject> Npc::getCreator() {
	AION_UNPORTED();
}

bool Npc::isFlag() {
	AION_UNPORTED();
}

bool Npc::isRaidMonster() {
	AION_UNPORTED();
}

int32_t Npc::getCancelLevel() {
	AION_UNPORTED();
}

bool Npc::isBoss() {
	AION_UNPORTED();
}

bool Npc::hasStatic() {
	AION_UNPORTED();
}

Race Npc::getRace() {
	AION_UNPORTED();
}

bool Npc::canSell() {
	AION_UNPORTED();
}

bool Npc::canBuy() {
	AION_UNPORTED();
}

bool Npc::canTradeIn() {
	AION_UNPORTED();
}

bool Npc::canPurchase() {
	AION_UNPORTED();
}

templates::npc::GroupDropType Npc::getGroupDrop() {
	AION_UNPORTED();
}

void Npc::overrideEquipmentList(std::unique_ptr<dataholders::loadingutils::adapters::NpcEquipmentList> v) {
	AION_UNPORTED();
}

runtime::Ptr<items::NpcEquippedGear> Npc::getOverrideEquipment() {
	// Java: overriddenEquipment != null ? overriddenEquipment : getObjectTemplate().getEquipment()
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::gameobjects
