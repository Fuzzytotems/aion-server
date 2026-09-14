#include "aion/gameserver/controllers/attack/AggroList.h"

#include "aion/gameserver/runtime/base/Unported.h"

// S0b transition (docs/design/hub-headers.md §3.3): the constructor and destructor need the owner type Creature (hub header of another S0b group)
// and the map element type AggroInfo; getFinalDamageList returns DamageList by value (S0c declaration headers). Remove the guards once the
// headers exist (spine freeze).
#if __has_include("aion/gameserver/model/gameobjects/Creature.h") && __has_include("aion/gameserver/controllers/attack/AggroInfo.h")
#define AION_S0B_AGGRO_LIST_MEMBERS 1
#include "aion/gameserver/controllers/attack/AggroInfo.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#else
#define AION_S0B_AGGRO_LIST_MEMBERS 0
#endif
#if __has_include("aion/gameserver/controllers/attack/DamageList.h")
#define AION_S0B_AGGRO_LIST_DAMAGE_LIST 1
#include "aion/gameserver/controllers/attack/DamageList.h"
#else
#define AION_S0B_AGGRO_LIST_DAMAGE_LIST 0
#endif

namespace aion::gameserver::controllers::attack {

#if AION_S0B_AGGRO_LIST_MEMBERS
AggroList::AggroList(model::gameobjects::Creature& ownerValue) : OwnedPart(ownerValue), owner(ownerValue) {
}

AggroList::~AggroList() = default;
#endif

void AggroList::addDamage(model::gameobjects::Creature& attacker, int32_t damage, bool notifyAttack,
	std::optional<skillengine::model::HopType> hopType) {
	AION_UNPORTED();
}

void AggroList::addHate(model::gameobjects::Creature& creature, int32_t hate) {
	AION_UNPORTED();
}

void AggroList::addDamageAndHate(model::gameobjects::Creature& creature, int32_t damage, int32_t hate) {
	AION_UNPORTED();
}

bool AggroList::shouldAddHateToMaster(model::gameobjects::Creature& creature) {
	AION_UNPORTED();
}

bool AggroList::isTauntingSpirit(model::gameobjects::SummonedObject& npc) {
	AION_UNPORTED();
}

runtime::Ptr<model::gameobjects::player::Player> AggroList::getMostPlayerDamage() {
	AION_UNPORTED();
}

void AggroList::stopHating(model::gameobjects::VisibleObject& creature) {
	AION_UNPORTED();
}

void AggroList::remove(model::gameobjects::Creature& creature) {
	AION_UNPORTED();
}

void AggroList::remove(model::gameobjects::Creature& creature, bool transferDamagesToMaster) {
	AION_UNPORTED();
}

void AggroList::transferDamagesToMaster(AggroInfo& aggroInfo) {
	AION_UNPORTED();
}

// lint: L7 unported stub; Java synchronizes it, the port adds SYNCHRONIZED
void AggroList::clear() {
	AION_UNPORTED();
}

bool AggroList::isHating(model::gameobjects::Creature& creature) {
	AION_UNPORTED();
}

int32_t AggroList::getHate(model::gameobjects::Creature& creature) {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<AggroInfo>> AggroList::stream() {
	AION_UNPORTED();
}

runtime::Ptr<model::gameobjects::Creature> AggroList::getTarget(AggroTarget targetType) {
	AION_UNPORTED();
}

runtime::Ptr<model::gameobjects::Creature> AggroList::getTarget(AggroTarget targetType, float range) {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<model::gameobjects::Creature>> AggroList::streamValidTargets(float range) {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<AggroInfo>> AggroList::streamValidTargetInfo(float range) {
	AION_UNPORTED();
}

#if AION_S0B_AGGRO_LIST_DAMAGE_LIST
DamageList AggroList::getFinalDamageList() {
	AION_UNPORTED();
}
#endif

bool AggroList::isAware(runtime::Ptr<model::gameobjects::Creature> creature) {
	AION_UNPORTED();
}

// callbacks: com.aionemu.gameserver.controllers.attack.AggroList@L206:77 (hate reduction task, stored as hateReductionTask)
// lint: L7 unported stub; Java synchronizes it, the port adds SYNCHRONIZED
void AggroList::startHateReductionTask() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::controllers::attack
