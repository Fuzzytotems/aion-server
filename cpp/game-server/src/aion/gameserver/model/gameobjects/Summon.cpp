#include "aion/gameserver/model/gameobjects/Summon.h"

#include <optional>
#include <string>

#include "aion/gameserver/controllers/SiegeWeaponController.h"
#include "aion/gameserver/controllers/SummonController.h"
#include "aion/gameserver/controllers/attack/PlayerAggroList.h"
#include "aion/gameserver/controllers/movement/SiegeWeaponMoveController.h"
#include "aion/gameserver/controllers/movement/SummonMoveController.h"
#include "aion/gameserver/model/CreatureType.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/SkillElement.h"
#include "aion/gameserver/model/TribeClass.h"
#include "aion/gameserver/model/gameobjects/NpcObjectType.h"
#include "aion/gameserver/model/gameobjects/detail/ObjectsData.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/stats/container/SummonGameStats.h"
#include "aion/gameserver/model/stats/container/SummonLifeStats.h"
#include "aion/gameserver/model/summons/SkillOrder.h"
#include "aion/gameserver/model/summons/SummonMode.h"
#include "aion/gameserver/model/summons/SummonRelease.h"
#include "aion/gameserver/model/summons/UnsummonType.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/npc/NpcTemplateType.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::model::gameobjects {

Summon::Summon(CreateKey key, int32_t objId, std::unique_ptr<controllers::SummonController> controller,
	templates::spawns::SpawnTemplate& spawnTemplate,
	const templates::npc::NpcTemplate* objectTemplate, player::Player& masterValue, int32_t time)
	: Creature(key, objId, std::move(controller), spawnTemplate, objectTemplate, world::WorldPosition::create(spawnTemplate.getWorldId()), true),
	  master(masterValue), liveTime(time) {
}

Summon::~Summon() = default;

void Summon::postConstruct() {
	Creature::postConstruct();
	controllers::SummonController& summonController = getController(); // Java: controller (a constructor parameter)
	summonController.setOwner(*this);
	if (dynamic_cast<controllers::SiegeWeaponController*>(&summonController) != nullptr)
		moveController.set(std::make_unique<controllers::movement::SiegeWeaponMoveController>(*this));
	else
		moveController.set(std::make_unique<controllers::movement::SummonMoveController>(*this));
	setGameStats(std::make_unique<stats::container::SummonGameStats>(*this));
	setLifeStats(std::make_unique<stats::container::SummonLifeStats>(*this));
	setAlwaysResistElement(getObjectTemplate());
}

std::unique_ptr<controllers::attack::AggroList> Summon::createAggroList() {
	return std::make_unique<controllers::attack::PlayerAggroList>(*this);
}

runtime::Ptr<stats::container::SummonGameStats> Summon::getGameStats() const {
	return runtime::cast<stats::container::SummonGameStats>(Creature::getGameStats());
}

runtime::Ptr<Creature> Summon::getMaster() {
	return master;
}

controllers::SummonController& Summon::getController() const {
	return static_cast<controllers::SummonController&>(Creature::getController());
}

runtime::Ptr<controllers::movement::SummonMoveController> Summon::getMoveController() const {
	return runtime::cast<controllers::movement::SummonMoveController>(Creature::getMoveController());
}

const templates::npc::NpcTemplate* Summon::getObjectTemplate() const {
	return static_cast<const templates::npc::NpcTemplate*>(Creature::getObjectTemplate());
}

void Summon::setAlwaysResistElement(const templates::npc::NpcTemplate* template_) {
	if (template_ != nullptr) {
		std::string name = template_->getName();
		if (name == "earth spirit")
			alwaysResistElement.set(SkillElement::EARTH);
		else if (name == "fire spirit")
			alwaysResistElement.set(SkillElement::FIRE);
		else if (name == "water spirit")
			alwaysResistElement.set(SkillElement::WATER);
		else if (name == "wind spirit")
			alwaysResistElement.set(SkillElement::WIND);
	}
}

int8_t Summon::getLevel() {
	return getObjectTemplate()->getLevel();
}

int32_t Summon::getNpcId() {
	return getObjectTemplate()->getTemplateId();
}

std::string Summon::getL10n() {
	return getObjectTemplate()->getL10n();
}

NpcObjectType Summon::getNpcObjectType() {
	return NpcObjectType::SUMMON;
}

summons::SummonMode Summon::getVisibleMode() {
	return isReleaseUncancelable() ? modeBeforeRelease.get() : mode.get();
}

void Summon::setMode(summons::SummonMode modeValue) {
	if (modeValue != summons::SummonMode::ATTACK)
		clearSkillOrders();
	if (mode.get() != summons::SummonMode::RELEASE)
		modeBeforeRelease.set(mode.get());
	mode.set(modeValue);
}

bool Summon::isEnemy(Creature& creature) {
	return master->isEnemy(creature);
}

bool Summon::isEnemyFrom(Npc& npc) {
	return master->isEnemyFrom(npc);
}

bool Summon::isEnemyFrom(player::Player& player) {
	return master->isEnemyFrom(player);
}

bool Summon::isPvpTarget(Creature& creature) {
	return static_cast<bool>(runtime::as<player::Player>(creature.getActingCreature()));
}

std::optional<TribeClass> Summon::getTribe() {
	return master->getTribe();
}

CreatureType Summon::getType(Creature& creature) {
	bool friend_ = master->getRace() == creature.getRace() && !creature.isEnemy(*master);
	return friend_ ? CreatureType::SUPPORT : CreatureType::ATTACKABLE;
}

runtime::Ptr<Creature> Summon::getActingCreature() {
	return getMaster();
}

Race Summon::getRace() {
	return getMaster()->getRace();
}

bool Summon::isPet() {
	return getObjectTemplate()->getNpcTemplateType() == templates::npc::NpcTemplateType::SUMMON_PET;
}

bool Summon::registerRelease(summons::SummonRelease& release) {
	// java-race: check-then-act on pendingRelease without a lock, as in Java (SummonsService calls it from the master's and the summon's tasks)
	runtime::Ptr<summons::SummonRelease> pending = pendingRelease.get();
	if (pending) {
		if (pending->hasStarted() || !detail::isInstant(release.getUnsummonType()))
			return false;
		pending->cancel();
	}
	pendingRelease.set(runtime::Ptr<summons::SummonRelease>(release));
	return true;
}

bool Summon::startRelease(summons::SummonRelease& release) {
	if (pendingRelease.get() != &release)
		return false;
	release.markStarted();
	return true;
}

void Summon::cancelReleaseByMaster() {
	runtime::Ptr<summons::SummonRelease> pending = pendingRelease.get();
	if (pending && pending->isCancelableByMaster() && pending->cancel())
		pendingRelease.set(nullptr);
}

bool Summon::isReleaseUncancelable() {
	runtime::Ptr<summons::SummonRelease> release = pendingRelease.get();
	return release && !release->isCancelableByMaster();
}

bool Summon::isBeingReleased() {
	return static_cast<bool>(pendingRelease.get());
}

void Summon::addSkillOrder(int32_t skillId, int32_t skillLvl, Creature& targetValue, int32_t hate, bool release) {
	skillOrders.add(summons::SkillOrder::create(skillId, skillLvl, targetValue, hate, release));
}

runtime::Ptr<summons::SkillOrder> Summon::retrieveNextSkillOrder() {
	return skillOrders.poll();
}

runtime::Ptr<summons::SkillOrder> Summon::getNextSkillOrder() {
	return skillOrders.peek();
}

void Summon::clearSkillOrders() {
	skillOrders.clear();
}

} // namespace aion::gameserver::model::gameobjects
