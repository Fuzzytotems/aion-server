#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK.h"

#include "aion/commons/utils/Exception.h"
#include "aion/gameserver/controllers/attack/AttackResult.h"
#include "aion/gameserver/controllers/attack/AttackStatus.h"
#include "aion/gameserver/model/animations/AttackHandAnimationInfo.h"
#include "aion/gameserver/model/animations/AttackTypeAnimationInfo.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/serverpackets/detail/PacketSupport.h"
#include "aion/gameserver/skillengine/model/Effect.h"

namespace aion::gameserver::network::aion::serverpackets {

namespace {

/** Java: list.get(0) of an empty list throws IndexOutOfBoundsException */
controllers::attack::AttackResult& firstAttack(const std::vector<runtime::Ref<controllers::attack::AttackResult>>& attackList) {
	if (attackList.empty())
		throw commons::utils::IndexOutOfBoundsException("Index 0 out of bounds for length 0");
	return *runtime::Ptr<controllers::attack::AttackResult>(attackList.front());
}

} // namespace

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
	writeD(attacker->getObjectId());
	writeC(attackno);
	writeH(time);
	writeC(model::animations::getId(attackTypeAnimation));
	writeC(model::animations::getId(attackHandAnimation));
	writeD(target->getObjectId());
	writeC(target->getLifeStats()->getHpPercentage());
	writeC(attacker->getLifeStats()->getHpPercentage());

	// TODO refactor attack controller
	switch (detail::attackStatusId(firstAttack(attackList).getAttackStatus())) // Counter skills
	{
		case -60: // case CRITICAL_BLOCK
		case 4: // case BLOCK
			writeH(32);
			break;
		case -62: // case CRITICAL_PARRY
		case 2: // case PARRY
			writeH(64);
			break;
		case -64: // case CRITICAL_DODGE
		case 0: // case DODGE
			writeH(128);
			break;
		case -58: // case PHYSICAL_CRITICAL_RESIST
		case 6: // case RESIST
			writeH(256); // need more info becuz sometimes 0
			break;
		default:
			if (criticalProcEffect) {
				if (runtime::as<model::gameobjects::player::Player>(target))
					writeH(criticalProcEffect->getSkillId() == 8218 ? 1 : 2);
				else
					writeH(criticalProcEffect->getSkillId() == 8218 ? 1025 : 1026);
			} else {
				writeH(0);
			}
			break;
	}

	// setting counter skill from packet to have the best synchronization of time with client
	if (runtime::Ptr<model::gameobjects::player::Player> targetPlayer = runtime::as<model::gameobjects::player::Player>(target)) {
		if (detail::attackStatusIsCounterSkill(firstAttack(attackList).getAttackStatus()))
			targetPlayer->setLastCounterSkill(firstAttack(attackList).getAttackStatus());
	}

	writeH(0);
	if (criticalProcEffect) {
		writeF(criticalProcEffect->getTargetX());
		writeF(criticalProcEffect->getTargetY());
		writeF(criticalProcEffect->getTargetZ());
	}

	// TODO! those 2h (== d) up is some kind of very weird flag...
	writeC(static_cast<int32_t>(attackList.size()));
	for (const runtime::Ref<controllers::attack::AttackResult>& attack : attackList) {
		writeD(attack->getDamage());
		writeC(detail::attackStatusId(attack->getAttackStatus()));
		int8_t shieldType = static_cast<int8_t>(attack->getShieldType());
		writeC(shieldType);

		// shield Type: 1: reflector 2: normal shield 8: protect effect (ex. skillId: 417 Bodyguard) TODO find out 4
		switch (shieldType) {
			case 0:
			case 2:
				break;
			case 8:
			case 10:
				writeD(attack->getProtectorId()); // protectorId
				writeD(attack->getProtectedDamage()); // protected damage
				writeD(attack->getProtectedSkillId()); // skillId
				break;
			case 16:
				writeD(0);
				writeD(0);
				writeD(0);
				writeD(0);
				writeD(0);
				writeD(attack->getMpAbsorbed());
				writeD(attack->getReflectedSkillId()); // skill id
				break;
			default:
				writeD(attack->getProtectorId()); // protectorId
				writeD(attack->getProtectedDamage()); // protected damage
				writeD(attack->getProtectedSkillId()); // skillId
				writeD(attack->getReflectedDamage()); // reflect damage
				writeD(attack->getReflectedSkillId()); // skill id
				writeD(0);
				writeD(0);
				break;
		}
	}
	writeC(0);
}

} // namespace aion::gameserver::network::aion::serverpackets
