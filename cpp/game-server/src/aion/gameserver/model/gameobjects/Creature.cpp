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
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/TribeClass.h"
#include "aion/gameserver/model/gameobjects/CreatureTemplate.h"
#include "aion/gameserver/model/gameobjects/NpcObjectType.h"
#include "aion/gameserver/model/gameobjects/Pet.h"
#include "aion/gameserver/model/gameobjects/TransformModel.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/state/CreatureSeeStateInfo.h"
#include "aion/gameserver/model/gameobjects/state/CreatureStateInfo.h"
#include "aion/gameserver/model/gameobjects/state/CreatureVisualStateInfo.h"
#include "aion/gameserver/model/stats/container/CreatureGameStats.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/model/templates/item/ItemAttackType.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/model/templates/zone/ZoneType.h"
#include "aion/gameserver/runtime/sync/LockClass.h"
#include "aion/gameserver/runtime/sync/Monitor.h"
#include "aion/gameserver/skillengine/condition/PlayerMovedCondition.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/world/MapRegion.h"
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
	return getLifeStats()->isDead();
}

bool Creature::isFlag() {
	return false;
}

bool Creature::isCasting() {
	return static_cast<bool>(castingSkill.get());
}

void Creature::setCasting(runtime::Ptr<skillengine::model::Skill> castingSkillValue) {
	if (castingSkillValue)
		skillNumber++;
	castingSkill.set(castingSkillValue);
}

int32_t Creature::getCastingSkillId() {
	runtime::Ptr<skillengine::model::Skill> skill = castingSkill.get();
	return skill ? skill->getSkillTemplate()->getSkillId() : 0;
}

bool Creature::isCastingItemSkill() {
	runtime::Ptr<skillengine::model::Skill> skill = castingSkill.get();
	return skill && skill->getItemTemplate() != nullptr;
}

int32_t Creature::getCancelLevel() {
	return 100;
}

void Creature::incrementAttackedCount() {
	attackedCount++;
}

void Creature::clearAttackedCount() {
	attackedCount.set(0);
}

bool Creature::canPerformMove() {
	return (!(getEffectController()->isInAnyAbnormalState(skillengine::effect::AbnormalState::CANT_MOVE_STATE) && isSpawned() && canUseSkillInMove()));
}

bool Creature::canUseSkillInMove() {
	runtime::Ptr<skillengine::model::Skill> skill = castingSkill.get();
	if (skill) {
		// Java: DataManager.SKILL_DATA.getSkillTemplate(castingSkill.getSkillId()), the template the skill was created from (SkillEngine and every
		// Skill constructor take it from SKILL_DATA). SkillData declares no getSkillTemplate before P4-09, so the skill's own template is read.
		const skillengine::model::SkillTemplate* st = skill->getSkillTemplate();
		if (st->getStartconditions() != nullptr && st->getMovedCondition() != nullptr) {
			if (!st->getMovedCondition()->isAllow())
				return false;
		}
	}
	return true;
}

bool Creature::canAttack() {
	return (!getEffectController()->isInAnyAbnormalState(skillengine::effect::AbnormalState::CANT_ATTACK_STATE) && !isCasting() &&
		!isInState(gameobjects::state::CreatureState::RESTING) && !isInState(gameobjects::state::CreatureState::PRIVATE_SHOP));
}

void Creature::setState(gameobjects::state::CreatureState stateValue) {
	setState(stateValue, false);
}

void Creature::setState(gameobjects::state::CreatureState stateValue, bool replace) {
	if (replace)
		state.set(getId(stateValue));
	else
		state |= getId(stateValue);
}

void Creature::unsetState(gameobjects::state::CreatureState stateValue) {
	state &= ~getId(stateValue);
}

bool Creature::isInState(gameobjects::state::CreatureState stateValue) {
	if (mustMatchExact(stateValue))
		return state.get() == getId(stateValue);
	else
		return (state.get() & getId(stateValue)) == getId(stateValue);
}

void Creature::setVisualState(gameobjects::state::CreatureVisualState visualStateValue) {
	visualState |= getId(visualStateValue);
}

void Creature::unsetVisualState(gameobjects::state::CreatureVisualState visualStateValue) {
	visualState &= ~getId(visualStateValue);
}

bool Creature::isInVisualState(gameobjects::state::CreatureVisualState visualStateValue) {
	return (visualState.get() & getId(visualStateValue)) == getId(visualStateValue);
}

bool Creature::isInAnyHide() {
	int32_t current = visualState.get();
	return current != getId(gameobjects::state::CreatureVisualState::VISIBLE) && current != getId(gameobjects::state::CreatureVisualState::BLINKING);
}

void Creature::setSeeState(gameobjects::state::CreatureSeeState seeStateValue) {
	seeState |= getId(seeStateValue);
}

void Creature::unsetSeeState(gameobjects::state::CreatureSeeState seeStateValue) {
	seeState &= ~getId(seeStateValue);
}

bool Creature::isInSeeState(gameobjects::state::CreatureSeeState seeStateValue) {
	int32_t isSeeState = seeState.get() & getId(seeStateValue);

	if (isSeeState == getId(seeStateValue))
		return true;

	return false;
}

TransformModel& Creature::getTransformModel() {
	if (!transformModel)
		transformModel.set(std::make_unique<TransformModel>(*this)); // java-race: two first calls may each create a model, the later one wins
	return *transformModel;
}

void Creature::endTransformation() {
	getTransformModel().apply(0);
}

bool Creature::isTransformed() {
	return transformModel && getTransformModel().isActive();
}

bool Creature::isEnemy(Creature& creature) {
	return creature.isEnemyFrom(*this);
}

bool Creature::isEnemyFrom(Creature& creature) {
	return false;
}

bool Creature::isEnemyFrom(player::Player& player) {
	return false;
}

bool Creature::isEnemyFrom(Npc& npc) {
	return false;
}

std::optional<TribeClass> Creature::getTribe() {
	return TribeClass::GENERAL;
}

TribeClass Creature::getBaseTribe() {
	return TribeClass::GENERAL;
}

bool Creature::canSee(runtime::Ptr<VisibleObject> object) {
	if (runtime::Ptr<Creature> creature = runtime::as<Creature>(object)) {
		int32_t visualStateExcludingBlinking = creature->getVisualState() & ~getId(gameobjects::state::CreatureVisualState::BLINKING);
		if (visualStateExcludingBlinking <= getSeeState())
			return true;
		runtime::Ptr<Creature> master = creature->getMaster();
		return master && equals(*master); // traps, summons, etc. should always be visible to the master
	} else if (runtime::Ptr<Pet> pet = runtime::as<Pet>(object)) {
		// we must prevent sending the pet's spawn packet to others before the master's, as this causes the pet to stay invisible
		runtime::Ptr<player::Player> petMaster = pet->getMaster();
		return equals(*petMaster) || canSee(petMaster) && getKnownList().sees(*petMaster);
	}
	return VisibleObject::canSee(object);
}

NpcObjectType Creature::getNpcObjectType() {
	return NpcObjectType::NORMAL;
}

runtime::Ptr<Creature> Creature::getMaster() {
	return *this;
}

runtime::Ptr<Creature> Creature::getActingCreature() {
	return getMaster();
}

bool Creature::isSkillDisabled(const skillengine::model::SkillTemplate* template_) {
	runtime::Ptr<runtime::RcConcurrentHashMap<int32_t, int64_t>> coolDowns = skillCoolDowns.get();
	if (!coolDowns)
		return false;

	int32_t cooldownId = template_->getCooldownId();
	std::optional<int64_t> coolDown = coolDowns->get(cooldownId);
	if (!coolDown) {
		return false;
	}

	if (*coolDown < commons::utils::currentTimeMillis()) {
		removeSkillCoolDown(cooldownId);
		return false;
	}
	return true;
}

int64_t Creature::getSkillCoolDown(int32_t cooldownId) {
	runtime::Ptr<runtime::RcConcurrentHashMap<int32_t, int64_t>> coolDowns = skillCoolDowns.get();
	return !coolDowns ? 0LL : coolDowns->getOrDefault(cooldownId, 0LL);
}

void Creature::setSkillCoolDown(int32_t cooldownId, int64_t time) {
	if (cooldownId == 0) {
		return;
	}
	runtime::Ptr<runtime::RcConcurrentHashMap<int32_t, int64_t>> coolDowns = skillCoolDowns.get();
	if (!coolDowns) {
		// Deviation (D6, docs/deviations/P4-11a.md): Java's `if (skillCoolDowns == null) skillCoolDowns = new ConcurrentHashMap<>()` lets two
		// first calls create two maps and lose a cooldown; the map is published with a compare-and-set, the losing caller uses the winner's map.
		runtime::Ref<runtime::RcConcurrentHashMap<int32_t, int64_t>> created =
			runtime::RcConcurrentHashMap<int32_t, int64_t>::create(AION_LOCK_CLASS(Creature::skillCoolDowns#stripe));
		runtime::Ptr<runtime::RcConcurrentHashMap<int32_t, int64_t>> createdPtr = created;
		coolDowns = skillCoolDowns.compareAndSet(nullptr, std::move(created)) ? createdPtr : skillCoolDowns.get();
	}
	coolDowns->put(cooldownId, time);
}

void Creature::removeSkillCoolDown(int32_t cooldownId) {
	runtime::Ptr<runtime::RcConcurrentHashMap<int32_t, int64_t>> coolDowns = skillCoolDowns.get();
	if (!coolDowns)
		return;
	coolDowns->remove(cooldownId);
}

bool Creature::isInvulnerable() {
	return false;
}

templates::item::ItemAttackType Creature::getAttackType() {
	return templates::item::ItemAttackType::PHYSICAL;
}

bool Creature::isFlying() {
	return (isInState(gameobjects::state::CreatureState::FLYING) && !isInState(gameobjects::state::CreatureState::RESTING)) ||
		isInState(gameobjects::state::CreatureState::GLIDING);
}

bool Creature::isInFlyingState() {
	return isInState(gameobjects::state::CreatureState::FLYING) && !isInState(gameobjects::state::CreatureState::RESTING);
}

bool Creature::isPvpTarget(Creature& creature) {
	return false;
}

std::vector<runtime::Ptr<world::zone::ZoneInstance>> Creature::findZones() {
	runtime::Ptr<world::MapRegion> mapRegion = getPosition()->getMapRegion();
	return !mapRegion ? std::vector<runtime::Ptr<world::zone::ZoneInstance>>() : mapRegion->findZones(*this);
}

void Creature::revalidateZones() {
	if (!isSpawned())
		return;
	runtime::Ptr<world::MapRegion> mapRegion = getPosition()->getMapRegion();
	if (mapRegion)
		mapRegion->revalidateZones(*this);
}

bool Creature::isInsideZone(const world::zone::ZoneName* zoneName) {
	if (!isSpawned())
		return false;
	return getPosition()->getMapRegion()->isInsideZone(zoneName, *this);
}

bool Creature::isInsideItemUseZone(const world::zone::ZoneName* zoneName) {
	if (!isSpawned())
		return false;
	return getPosition()->getMapRegion()->isInsideItemUseZone(zoneName, *this);
}

void Creature::setInsideZoneType(templates::zone::ZoneType zoneType) {
	SYNCHRONIZED(*zoneTypes) {
		(*zoneTypes)[static_cast<int32_t>(zoneType)]++;
	}
}

void Creature::unsetInsideZoneType(templates::zone::ZoneType zoneType) {
	SYNCHRONIZED(*zoneTypes) {
		(*zoneTypes)[static_cast<int32_t>(zoneType)]--;
	}
}

bool Creature::isInsideZoneType(templates::zone::ZoneType zoneType) {
	SYNCHRONIZED(*zoneTypes) {
		return (*zoneTypes)[static_cast<int32_t>(zoneType)].get() > 0;
	}
}

bool Creature::isInsidePvPZone() {
	SYNCHRONIZED(*zoneTypes) {
		if ((*zoneTypes)[static_cast<int32_t>(templates::zone::ZoneType::SIEGE)].get() > 0) {
			return true;
		}
		int32_t pvpValue = (*zoneTypes)[static_cast<int32_t>(templates::zone::ZoneType::PVP)].get();
		return pvpValue == 0 || pvpValue == 2;
	}
}

Race Creature::getRace() {
	return Race::NONE;
}

int32_t Creature::getSkillCooldown(const skillengine::model::SkillTemplate* template_) {
	return template_->getCooldown();
}

int64_t Creature::getMillisSinceSpawn() {
	return commons::utils::currentTimeMillis() - spawnTime;
}

bool Creature::isNewSpawn() {
	return getMillisSinceSpawn() < 1500;
}

bool Creature::isRaidMonster() {
	return false;
}

bool Creature::isWorldRaidMonster() {
	return getTribe() == TribeClass::WORLDRAID_MONSTER || getTribe() == TribeClass::WORLDRAID_MONSTER_SANDWORMSUM && isRaidMonster();
}

runtime::Ptr<items::NpcEquippedGear> Creature::getOverrideEquipment() {
	return nullptr;
}

} // namespace aion::gameserver::model::gameobjects
