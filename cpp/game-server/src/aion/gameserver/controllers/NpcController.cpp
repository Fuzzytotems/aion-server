#include "aion/gameserver/controllers/NpcController.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

// S0b transition (docs/design/hub-headers.md §3.3): the narrowing accessor needs Npc (hub header of another S0b group). Remove the guard once it
// exists (spine freeze).
#if __has_include("aion/gameserver/model/gameobjects/Npc.h")
#define AION_S0B_NPC_CONTROLLER_OWNER 1
#include "aion/gameserver/model/gameobjects/Npc.h"
#else
#define AION_S0B_NPC_CONTROLLER_OWNER 0
#endif

namespace aion::gameserver::controllers {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.controllers.NpcController");

NpcController::NpcController() = default;

NpcController::~NpcController() = default;

#if AION_S0B_NPC_CONTROLLER_OWNER
model::gameobjects::Npc& NpcController::getOwner() const {
	return static_cast<model::gameobjects::Npc&>(CreatureController::getOwner());
}
#endif

void NpcController::see(model::gameobjects::VisibleObject& object) {
	AION_UNPORTED();
}

void NpcController::notSee(model::gameobjects::VisibleObject& object, model::animations::ObjectDeleteAnimation animation) {
	AION_UNPORTED();
}

// callbacks: com.aionemu.gameserver.controllers.NpcController@L84:46 (scheduled think() task)
void NpcController::onTargetChanged(runtime::Ptr<model::gameobjects::VisibleObject> oldTarget,
	runtime::Ptr<model::gameobjects::VisibleObject> newTarget) {
	AION_UNPORTED();
}

void NpcController::onBeforeSpawn() {
	AION_UNPORTED();
}

void NpcController::onAfterSpawn() {
	AION_UNPORTED();
}

void NpcController::onDespawn() {
	AION_UNPORTED();
}

void NpcController::onDie(model::gameobjects::Creature& lastAttacker) {
	AION_UNPORTED();
}

void NpcController::petLoot(model::gameobjects::Npc& value) {
	AION_UNPORTED();
}

runtime::Ptr<model::gameobjects::Pet> NpcController::findPetForLooting(model::gameobjects::Npc& npc) {
	AION_UNPORTED();
}

void NpcController::doReward() {
	AION_UNPORTED();
}

void NpcController::onDialogRequest(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void NpcController::onDialogSelect(int32_t dialogActionId, int32_t prevDialogId, model::gameobjects::player::Player& player, int32_t questId,
	int32_t extendedRewardIndex) {
	AION_UNPORTED();
}

void NpcController::onAddHate(model::gameobjects::Creature& attacker, bool isNewInAggroList) {
	AION_UNPORTED();
}

void NpcController::onAttack(model::gameobjects::Creature& attacker, runtime::Ptr<skillengine::model::Effect> effect,
	network::aion::serverpackets::SM_ATTACK_STATUS_TYPE type, int32_t damage, bool notifyAttack,
	network::aion::serverpackets::SM_ATTACK_STATUS_LOG logId, std::optional<attack::AttackStatus> attackStatus,
	std::optional<skillengine::model::HopType> hopType) {
	AION_UNPORTED();
}

void NpcController::onStartMove() {
	AION_UNPORTED();
}

void NpcController::onStopMove() {
	AION_UNPORTED();
}

void NpcController::onEnterZone(world::zone::ZoneInstance& zoneInstance) {
	AION_UNPORTED();
}

bool NpcController::useSkill(int32_t skillId, int32_t skillLevel) {
	AION_UNPORTED();
}

void NpcController::loseAggro(bool restoreHp) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::controllers
