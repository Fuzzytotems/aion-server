#include "aion/gameserver/model/gameobjects/Summon.h"

#include "aion/gameserver/controllers/SiegeWeaponController.h"
#include "aion/gameserver/controllers/SummonController.h"
#include "aion/gameserver/controllers/attack/PlayerAggroList.h"
#include "aion/gameserver/controllers/movement/SiegeWeaponMoveController.h"
#include "aion/gameserver/controllers/movement/SummonMoveController.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/stats/container/SummonGameStats.h"
#include "aion/gameserver/model/stats/container/SummonLifeStats.h"
#include "aion/gameserver/model/summons/SkillOrder.h"
#include "aion/gameserver/model/summons/SummonRelease.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
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
	AION_UNPORTED();
}

int8_t Summon::getLevel() {
	AION_UNPORTED();
}

int32_t Summon::getNpcId() {
	AION_UNPORTED();
}

std::string Summon::getL10n() {
	AION_UNPORTED();
}

NpcObjectType Summon::getNpcObjectType() {
	AION_UNPORTED();
}

summons::SummonMode Summon::getVisibleMode() {
	AION_UNPORTED();
}

void Summon::setMode(summons::SummonMode modeValue) {
	AION_UNPORTED();
}

bool Summon::isEnemy(Creature& creature) {
	AION_UNPORTED();
}

bool Summon::isEnemyFrom(Npc& npc) {
	AION_UNPORTED();
}

bool Summon::isEnemyFrom(player::Player& player) {
	AION_UNPORTED();
}

bool Summon::isPvpTarget(Creature& creature) {
	AION_UNPORTED();
}

std::optional<TribeClass> Summon::getTribe() {
	AION_UNPORTED();
}

CreatureType Summon::getType(Creature& creature) {
	AION_UNPORTED();
}

runtime::Ptr<Creature> Summon::getActingCreature() {
	AION_UNPORTED();
}

Race Summon::getRace() {
	AION_UNPORTED();
}

bool Summon::isPet() {
	AION_UNPORTED();
}

bool Summon::registerRelease(summons::SummonRelease& release) {
	AION_UNPORTED();
}

bool Summon::startRelease(summons::SummonRelease& release) {
	AION_UNPORTED();
}

void Summon::cancelReleaseByMaster() {
	AION_UNPORTED();
}

bool Summon::isReleaseUncancelable() {
	AION_UNPORTED();
}

bool Summon::isBeingReleased() {
	AION_UNPORTED();
}

void Summon::addSkillOrder(int32_t skillId, int32_t skillLvl, Creature& targetValue, int32_t hate, bool release) {
	AION_UNPORTED();
}

runtime::Ptr<summons::SkillOrder> Summon::retrieveNextSkillOrder() {
	AION_UNPORTED();
}

runtime::Ptr<summons::SkillOrder> Summon::getNextSkillOrder() {
	AION_UNPORTED();
}

void Summon::clearSkillOrders() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::gameobjects
