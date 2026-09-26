#include "aion/gameserver/network/aion/serverpackets/SM_CASTSPELL_RESULT.h"

#include "aion/gameserver/controllers/attack/AttackStatus.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/serverpackets/detail/PacketSupport.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/EffectReserved.h"
#include "aion/gameserver/skillengine/model/EffectResult.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/skillengine/model/SpellStatus.h"
#include "aion/gameserver/skillengine/model/SubEffectType.h"

namespace aion::gameserver::network::aion::serverpackets {

namespace {

/** Java: EffectResult.getId() - NORMAL(0) .. CANCELED_DUE_TO_TOO_MANY_EFFECTS(6): equal to the ordinal */
int32_t effectResultId(skillengine::model::EffectResult result) {
	return static_cast<int32_t>(result);
}

/** Java: SpellStatus.getId() in ordinal order - NONE(0), STUMBLE(1), STAGGER(2), OPENAERIAL(4), CLOSEAERIAL(8), SPIN(16), BLOCK(32), PARRY(64), DODGE(128), DODGE2(-128), RESIST(256) */
int32_t spellStatusId(skillengine::model::SpellStatus status) {
	static constexpr int32_t IDS[] = {0, 1, 2, 4, 8, 16, 32, 64, 128, -128, 256};
	return IDS[static_cast<size_t>(status)];
}

} // namespace

SM_CASTSPELL_RESULT::SM_CASTSPELL_RESULT(skillengine::model::Skill& skillValue,
	const std::vector<runtime::Ptr<skillengine::model::Effect>>& effectsValue, int32_t hitTimeValue, bool chainSuccessValue, int32_t dashStatusValue)
	: AionServerPacket(opcodeOf<SM_CASTSPELL_RESULT>), effector(skillValue.getEffector()), target(skillValue.getFirstTarget()), skill(skillValue),
	  cooldown(skillValue.getCooldown()), hitTime(hitTimeValue), effects(effectsValue.begin(), effectsValue.end()), dashStatus(dashStatusValue),
	  targetType(0), chainSuccess(chainSuccessValue) {
}

SM_CASTSPELL_RESULT::SM_CASTSPELL_RESULT(skillengine::model::Skill& skillValue,
	const std::vector<runtime::Ptr<skillengine::model::Effect>>& effectsValue, int32_t hitTimeValue, bool chainSuccessValue, int32_t dashStatusValue,
	int32_t targetTypeValue)
	: SM_CASTSPELL_RESULT(skillValue, effectsValue, hitTimeValue, chainSuccessValue, dashStatusValue) {
	this->targetType = targetTypeValue;
}

SM_CASTSPELL_RESULT::~SM_CASTSPELL_RESULT() = default;

void SM_CASTSPELL_RESULT::writeImpl(AionConnection* con) {
	using controllers::attack::AttackStatus;
	using skillengine::model::EffectResult;
	using skillengine::model::SubEffectType;
	writeD(effector->getObjectId());
	writeC(targetType);
	switch (targetType) {
		case 0:
		case 3:
		case 4:
			writeD(target->getObjectId());
			break;
		case 1:
			writeF(skill->getX());
			writeF(skill->getY());
			writeF(skill->getZ());
			break;
		case 2:
			writeF(skill->getX());
			writeF(skill->getY());
			writeF(skill->getZ());
			writeF(0); // unk1
			writeF(0); // unk2
			writeF(0); // unk3
			writeF(0); // unk4
			writeF(0); // unk5
			writeF(0); // unk6
			writeF(0); // unk7
			writeF(0); // unk8
			break;
	}
	writeH(skill->getSkillTemplate()->getSkillId());
	writeC(skill->getSkillTemplate()->getLvl());
	writeD(cooldown);
	writeH(hitTime);
	writeC(0); // unk

	// 0 : no chain skill 16 : no damage to all target like dodge, resist or effect size is 0 32 : regular Seen: 0xA0 for skill 2723, 0x22 for skill
	// 1169; Skill id doesn't fit to this structure: 2395
	if (effects.empty())
		writeC(16);
	else if (chainSuccess)
		writeC(32);
	else
		writeC(0);

	if (skill->getItemTemplate() != nullptr && skill->getItemTemplate()->isCombatActivated()) {
		writeC(2);
		writeD(skill->getItemObjectId());
		writeD(skill->getItemTemplate()->getTemplateId());
		writeC(0); // unk 0
	} else {
		if (skill->getSkillMethod() == skillengine::model::Skill::SkillMethod::PENALTY)
			writeC(4);
		else
			writeC(0);
		writeC(this->dashStatus);
		switch (this->dashStatus) {
			case 1:
			case 2:
			case 3:
			case 4:
			case 6:
				writeC(skill->getH());
				writeF(skill->getX());
				writeF(skill->getY());
				writeF(skill->getZ());
				break;
		}
	}

	writeH(static_cast<int32_t>(effects.size()));
	for (const runtime::Ref<skillengine::model::Effect>& effect : effects) {
		runtime::Ptr<model::gameobjects::Creature> effected = effect->getOriginalEffected();
		if (effected) {
			writeD(effected->getObjectId());
			writeC(effectResultId(effect->getEffectResult())); // 0 - NORMAL, 1 - ABSORBED, 2 - CONFLICT, 3 - DODGE, 4 - RESIST
			writeC(effect->getEffectedHp() == -1 ? effected->getLifeStats()->getHpPercentage() : effect->getEffectedHp()); // target %hp
		} else { // point point skills
			writeD(effector->getObjectId());
			writeC(0);
			writeC(100);
		}
		writeC(effector->getLifeStats()->getHpPercentage()); // attacker %hp

		// Spell Status 1 : stumble 2 : knockback 4 : open aerial 8 : close aerial 16 : spin 32 : block 64 : parry 128 : dodge 256 : resist
		writeC(spellStatusId(effect->getSpellStatus()));
		writeC(effect->getSuccessfulEffectsAsByte());
		writeH(0);
		writeC(effect->getCarvedSignet()); // current carve signet count

		switch (spellStatusId(effect->getSpellStatus())) {
			case 1:
			case 2:
			case 4:
			case 8:
				writeF(effect->getTargetX());
				writeF(effect->getTargetY());
				writeF(effect->getTargetZ());
				break;
			case 16:
				writeC(effect->getEffector()->getHeading());
				break;
			default:
				switch (effect->getSubEffectType()) {
					case SubEffectType::PULL:
					case SubEffectType::PULL_NPC:
					case SubEffectType::SIMPLE_MOVE_BACK:
						writeF(effect->getTargetX());
						writeF(effect->getTargetY());
						writeF(effect->getTargetZ());
						break;
					default:
						break;
				}
				break;
		}

		std::vector<runtime::Ref<skillengine::model::EffectReserved>> reservedEffects = effect->getReservedEffectsToSend();
		writeC(static_cast<int32_t>(reservedEffects.size())); // it's success effect count (MP and HP heal, for example, always atleast 1)
		for (const runtime::Ref<skillengine::model::EffectReserved>& er : reservedEffects) {
			writeC(static_cast<int32_t>(er->getType())); // HP - 0 , MP - 1, FP - 2, DP - 3? (Java: ResourceType.getValue() equals the ordinal)
			writeD(er->getValueToSend());
			writeC(detail::attackStatusId(er->getAttackStatus()));
			bool isCounter = detail::attackStatusIsCounterSkill(effect->getAttackStatus());
			if (effect->getEffectResult() == EffectResult::RESIST)
				isCounter = true;

			// setting counter skill from packet to have the best synchronization of time with client
			if (runtime::Ptr<model::gameobjects::player::Player> effectedPlayer = runtime::as<model::gameobjects::player::Player>(effect->getEffected())) {
				if (isCounter)
					effectedPlayer->setLastCounterSkill(effect->getEffectResult() == EffectResult::RESIST ? AttackStatus::RESIST : effect->getAttackStatus());
			}

			// shield Type: 1: reflector 2: normal shield 8: protect effect (ex. skillId: 417 Bodyguard) 16: mp shield TODO find out 4
			writeC(effect->getShieldDefense());

			switch (effect->getShieldDefense()) {
				case 0:
				case 2:
					break;
				case 8:
				case 10:
					writeD(effect->getProtectorId()); // protectorId
					writeD(effect->getProtectedDamage()); // protected damage
					writeD(effect->getProtectedSkillId()); // skillId
					break;
				default:
					writeD(effect->getProtectorId()); // protectorId
					writeD(effect->getProtectedDamage()); // protected damage
					writeD(effect->getProtectedSkillId()); // skillId
					writeD(effect->getReflectedDamage()); // reflect damage
					writeD(effect->getReflectedSkillId()); // skill id
					writeD(effect->getMpAbsorbed());
					writeD(effect->getMpShieldSkillId()); // skill id
					break;
			}
		}
	}
}

} // namespace aion::gameserver::network::aion::serverpackets
