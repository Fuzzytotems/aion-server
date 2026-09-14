#include "aion/gameserver/model/gameobjects/Creature.h"

#include <optional>
#include <string>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/ai/AIEngine.h"
#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/controllers/CreatureController.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/VisibleObjectController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/controllers/movement/CreatureMoveController.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/model/gameobjects/CreatureTemplate.h"
#include "aion/gameserver/model/gameobjects/TransformModel.h"
#include "aion/gameserver/model/stats/container/CreatureGameStats.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/model/templates/zone/ZoneType.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::model::gameobjects {

Creature::Creature(CreateKey key, int32_t objId, std::unique_ptr<controllers::CreatureController> controller,
	runtime::Ptr<templates::spawns::SpawnTemplate> spawnTemplate, const CreatureTemplate* objectTemplate, runtime::Ptr<world::WorldPosition> position,
	bool autoReleaseObjectId)
	: VisibleObject(key, objId, std::move(controller), spawnTemplate, objectTemplate, position, autoReleaseObjectId),
	  state(1),       // CreatureState.ACTIVE.getId()
	  visualState(0), // CreatureVisualState.VISIBLE.getId()
	  seeState(0),    // CreatureSeeState.NORMAL.getId()
	  // Java: this.observeController = new ObserveController() in the constructor body (no dependency on the dynamic type)
	  observeController(controllers::ObserveController::create()),
	  zoneTypes(runtime::Array<int8_t>::make(static_cast<int32_t>(xml::EnumTraits<templates::zone::ZoneType>::names.size()))),
	  spawnTime(commons::utils::currentTimeMillis()) {
}

Creature::~Creature() = default;

void Creature::postConstruct() {
	VisibleObject::postConstruct();
	std::optional<std::string> aiName = static_cast<const CreatureTemplate*>(getObjectTemplate())->getAiName();
	runtime::Ptr<templates::spawns::SpawnTemplate> spawn = getSpawn();
	if (spawn) {
		std::optional<std::string> spawnAiName = spawn->getAiName();
		if (spawnAiName)
			aiName = *spawnAiName == templates::spawns::SpawnTemplate::NO_AI ? std::nullopt : spawnAiName;
	}
	ai.set(gameserver::ai::AIEngine::getInstance().newAI(aiName, *this));
	// Java: this.observeController = new ObserveController() runs here; C++ creates it in the constructor (it is a const Ref)
	aggroList.set(createAggroList());
}

runtime::Ptr<controllers::movement::CreatureMoveController> Creature::getMoveController() const {
	return runtime::Ptr<controllers::movement::CreatureMoveController>(moveController.get());
}

std::unique_ptr<controllers::attack::AggroList> Creature::createAggroList() {
	return std::make_unique<controllers::attack::AggroList>(*this);
}

controllers::CreatureController& Creature::getController() const {
	return static_cast<controllers::CreatureController&>(VisibleObject::getController());
}

runtime::Ptr<stats::container::CreatureLifeStats> Creature::getLifeStats() const {
	return runtime::Ptr<stats::container::CreatureLifeStats>(lifeStats.get());
}

void Creature::setLifeStats(std::unique_ptr<stats::container::CreatureLifeStats> value) {
	lifeStats.set(std::move(value));
}

runtime::Ptr<stats::container::CreatureGameStats> Creature::getGameStats() const {
	return runtime::Ptr<stats::container::CreatureGameStats>(gameStats.get());
}

void Creature::setGameStats(std::unique_ptr<stats::container::CreatureGameStats> value) {
	gameStats.set(std::move(value));
}

runtime::Ptr<controllers::effect::EffectController> Creature::getEffectController() const {
	return runtime::Ptr<controllers::effect::EffectController>(effectController.get());
}

void Creature::setEffectController(std::unique_ptr<controllers::effect::EffectController> value) {
	effectController.set(std::move(value));
}

gameserver::ai::AbstractAI& Creature::getAi() const {
	return *ai;
}

void Creature::replaceAi(std::unique_ptr<gameserver::ai::AbstractAI> value) {
	ai.set(std::move(value));
}

controllers::attack::AggroList& Creature::getAggroList() const {
	return *aggroList;
}

bool Creature::isDead() {
	AION_UNPORTED();
}

bool Creature::isFlag() {
	AION_UNPORTED();
}

bool Creature::isCasting() {
	AION_UNPORTED();
}

void Creature::setCasting(runtime::Ptr<skillengine::model::Skill> castingSkillValue) {
	AION_UNPORTED();
}

int32_t Creature::getCastingSkillId() {
	AION_UNPORTED();
}

bool Creature::isCastingItemSkill() {
	AION_UNPORTED();
}

int32_t Creature::getCancelLevel() {
	AION_UNPORTED();
}

void Creature::incrementAttackedCount() {
	AION_UNPORTED();
}

void Creature::clearAttackedCount() {
	AION_UNPORTED();
}

bool Creature::canPerformMove() {
	AION_UNPORTED();
}

bool Creature::canUseSkillInMove() {
	AION_UNPORTED();
}

bool Creature::canAttack() {
	AION_UNPORTED();
}

void Creature::setState(gameobjects::state::CreatureState stateValue) {
	AION_UNPORTED();
}

void Creature::setState(gameobjects::state::CreatureState stateValue, bool replace) {
	AION_UNPORTED();
}

void Creature::unsetState(gameobjects::state::CreatureState stateValue) {
	AION_UNPORTED();
}

bool Creature::isInState(gameobjects::state::CreatureState stateValue) {
	AION_UNPORTED();
}

void Creature::setVisualState(gameobjects::state::CreatureVisualState visualStateValue) {
	AION_UNPORTED();
}

void Creature::unsetVisualState(gameobjects::state::CreatureVisualState visualStateValue) {
	AION_UNPORTED();
}

bool Creature::isInVisualState(gameobjects::state::CreatureVisualState visualStateValue) {
	AION_UNPORTED();
}

bool Creature::isInAnyHide() {
	AION_UNPORTED();
}

void Creature::setSeeState(gameobjects::state::CreatureSeeState seeStateValue) {
	AION_UNPORTED();
}

void Creature::unsetSeeState(gameobjects::state::CreatureSeeState seeStateValue) {
	AION_UNPORTED();
}

bool Creature::isInSeeState(gameobjects::state::CreatureSeeState seeStateValue) {
	AION_UNPORTED();
}

TransformModel& Creature::getTransformModel() {
	AION_UNPORTED();
}

void Creature::endTransformation() {
	AION_UNPORTED();
}

bool Creature::isTransformed() {
	AION_UNPORTED();
}

bool Creature::isEnemy(Creature& creature) {
	AION_UNPORTED();
}

bool Creature::isEnemyFrom(Creature& creature) {
	AION_UNPORTED();
}

bool Creature::isEnemyFrom(player::Player& player) {
	AION_UNPORTED();
}

bool Creature::isEnemyFrom(Npc& npc) {
	AION_UNPORTED();
}

std::optional<TribeClass> Creature::getTribe() {
	AION_UNPORTED();
}

TribeClass Creature::getBaseTribe() {
	AION_UNPORTED();
}

bool Creature::canSee(runtime::Ptr<VisibleObject> object) {
	AION_UNPORTED();
}

NpcObjectType Creature::getNpcObjectType() {
	AION_UNPORTED();
}

runtime::Ptr<Creature> Creature::getMaster() {
	AION_UNPORTED();
}

runtime::Ptr<Creature> Creature::getActingCreature() {
	AION_UNPORTED();
}

bool Creature::isSkillDisabled(const skillengine::model::SkillTemplate* template_) {
	AION_UNPORTED();
}

int64_t Creature::getSkillCoolDown(int32_t cooldownId) {
	AION_UNPORTED();
}

void Creature::setSkillCoolDown(int32_t cooldownId, int64_t time) {
	AION_UNPORTED();
}

void Creature::removeSkillCoolDown(int32_t cooldownId) {
	AION_UNPORTED();
}

bool Creature::isInvulnerable() {
	AION_UNPORTED();
}

templates::item::ItemAttackType Creature::getAttackType() {
	AION_UNPORTED();
}

bool Creature::isFlying() {
	AION_UNPORTED();
}

bool Creature::isInFlyingState() {
	AION_UNPORTED();
}

bool Creature::isPvpTarget(Creature& creature) {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<world::zone::ZoneInstance>> Creature::findZones() {
	AION_UNPORTED();
}

void Creature::revalidateZones() {
	AION_UNPORTED();
}

bool Creature::isInsideZone(const world::zone::ZoneName* zoneName) {
	AION_UNPORTED();
}

bool Creature::isInsideItemUseZone(const world::zone::ZoneName* zoneName) {
	AION_UNPORTED();
}

void Creature::setInsideZoneType(templates::zone::ZoneType zoneType) {
	AION_UNPORTED();
}

void Creature::unsetInsideZoneType(templates::zone::ZoneType zoneType) {
	AION_UNPORTED();
}

bool Creature::isInsideZoneType(templates::zone::ZoneType zoneType) {
	AION_UNPORTED();
}

bool Creature::isInsidePvPZone() {
	AION_UNPORTED();
}

Race Creature::getRace() {
	AION_UNPORTED();
}

int32_t Creature::getSkillCooldown(const skillengine::model::SkillTemplate* template_) {
	AION_UNPORTED();
}

int64_t Creature::getMillisSinceSpawn() {
	AION_UNPORTED();
}

bool Creature::isNewSpawn() {
	AION_UNPORTED();
}

bool Creature::isRaidMonster() {
	AION_UNPORTED();
}

bool Creature::isWorldRaidMonster() {
	AION_UNPORTED();
}

runtime::Ptr<items::NpcEquippedGear> Creature::getOverrideEquipment() {
	return nullptr;
}

} // namespace aion::gameserver::model::gameobjects
