#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK.h"

#include "aion/gameserver/controllers/attack/AttackResult.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/skillengine/model/Effect.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_ATTACK::SM_ATTACK(model::gameobjects::Creature& attackerValue, model::gameobjects::Creature& targetValue, int32_t attacknoValue, int32_t timeValue,
	model::animations::AttackTypeAnimation attackTypeAnimationValue, model::animations::AttackHandAnimation attackHandAnimationValue,
	const std::vector<runtime::Ptr<controllers::attack::AttackResult>>& attackListValue)
	: SM_ATTACK(attackerValue, targetValue, attacknoValue, timeValue, attackTypeAnimationValue, attackHandAnimationValue, attackListValue, nullptr) {
}

SM_ATTACK::SM_ATTACK(model::gameobjects::Creature& attackerValue, model::gameobjects::Creature& targetValue, int32_t attacknoValue, int32_t timeValue,
	model::animations::AttackTypeAnimation attackTypeAnimationValue, model::animations::AttackHandAnimation attackHandAnimationValue,
	const std::vector<runtime::Ptr<controllers::attack::AttackResult>>& attackListValue,
	runtime::Ptr<skillengine::model::Effect> criticalProcEffectValue)
	: AionServerPacket(opcodeOf<SM_ATTACK>), attackno(attacknoValue), time(timeValue), attackHandAnimation(attackHandAnimationValue),
	  attackTypeAnimation(attackTypeAnimationValue), attackList(attackListValue.begin(), attackListValue.end()), attacker(attackerValue),
	  target(targetValue), criticalProcEffect(criticalProcEffectValue) {
}

SM_ATTACK::~SM_ATTACK() = default;

void SM_ATTACK::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
