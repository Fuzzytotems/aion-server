#include "aion/gameserver/custom/instance/neuralnetwork/PlayerModelEntry.h"

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/model/PlayerClassInfo.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/skillengine/model/SkillSubType.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/utils/PositionUtil.h"

namespace aion::gameserver::custom::instance::neuralnetwork {

using model::gameobjects::Persistable_PersistentState;
using skillengine::effect::AbnormalState;

PlayerModelEntry::PlayerModelEntry(const Values& v, Persistable_PersistentState state)
	: timestamp(v.timestamp), skillID(v.skillID), playerID(v.playerID), playerClassID(v.playerClassID), playerHPpercentage(v.playerHPpercentage),
	  playerMPpercentage(v.playerMPpercentage), playerIsRooted(v.playerIsRooted), playerIsSilenced(v.playerIsSilenced),
	  playerIsBound(v.playerIsBound), playerIsStunned(v.playerIsStunned), playerIsAetherhold(v.playerIsAetherhold),
	  playerBuffCount(v.playerBuffCount), playerDebuffCount(v.playerDebuffCount), playerIsShielded(v.playerIsShielded),
	  targetHPpercentage(v.targetHPpercentage), targetMPpercentage(v.targetMPpercentage), targetFocusesPlayer(v.targetFocusesPlayer),
	  distance(v.distance), targetIsRooted(v.targetIsRooted), targetIsSilenced(v.targetIsSilenced), targetIsBound(v.targetIsBound),
	  targetIsStunned(v.targetIsStunned), targetIsAetherhold(v.targetIsAetherhold), targetBuffCount(v.targetBuffCount),
	  targetDebuffCount(v.targetDebuffCount), targetIsShielded(v.targetIsShielded) {
	setPersistentState(state);
}

// Java PlayerModelEntry.java:40-101 (the live constructor's values)
PlayerModelEntry::Values PlayerModelEntry::liveValues(model::gameobjects::Creature& playerOrBoss, int32_t skillID,
	runtime::Ptr<model::gameobjects::Creature> target) {
	Values v;
	v.timestamp = commons::database::Timestamp{std::chrono::milliseconds(commons::utils::currentTimeMillis())};
	v.skillID = skillID;
	v.playerID = playerOrBoss.getObjectId();
	runtime::Ptr<model::gameobjects::player::Player> player = runtime::as<model::gameobjects::player::Player>(playerOrBoss);
	v.playerClassID = player != nullptr ? model::getClassId(player->getPlayerClass()) : -1;
	v.playerHPpercentage = static_cast<float>(playerOrBoss.getLifeStats()->getHpPercentage());
	v.playerMPpercentage = static_cast<float>(playerOrBoss.getLifeStats()->getMpPercentage());
	v.playerIsRooted = playerOrBoss.getEffectController()->isAbnormalSet(AbnormalState::ROOT);
	v.playerIsSilenced = playerOrBoss.getEffectController()->isAbnormalSet(AbnormalState::SILENCE);
	v.playerIsBound = playerOrBoss.getEffectController()->isAbnormalSet(AbnormalState::BIND);
	v.playerIsStunned = playerOrBoss.getEffectController()->isAbnormalSet(AbnormalState::ANY_STUN);
	v.playerIsAetherhold = playerOrBoss.getEffectController()->isAbnormalSet(AbnormalState::OPENAERIAL);
	v.playerIsShielded = playerOrBoss.getEffectController()->isUnderNormalShield();
	v.playerBuffCount = 0;
	v.playerDebuffCount = 0;
	for (runtime::Ptr<skillengine::model::Effect> e : playerOrBoss.getEffectController()->getAbnormalEffects()) {
		if ((e->getSkillTemplate() != nullptr && e->getSkillTemplate()->getSubType() == skillengine::model::SkillSubType::BUFF) || // buff skills
			(e->getSkill() != nullptr && e->getSkill()->getItemObjectId() != 0)) // buff items
			v.playerBuffCount++;
		else // debuffs
			v.playerDebuffCount++;
	}
	if (target != nullptr) {
		if (target->getLifeStats()->getMaxHp() > 0)
			v.targetHPpercentage = static_cast<float>(target->getLifeStats()->getHpPercentage());
		else
			v.targetHPpercentage = 0;
		if (target->getLifeStats()->getMaxMp() > 0)
			v.targetMPpercentage = static_cast<float>(target->getLifeStats()->getMpPercentage());
		else
			v.targetMPpercentage = 0;
		v.targetFocusesPlayer = target->getTarget().get() == &playerOrBoss;
		v.distance = static_cast<float>(utils::PositionUtil::getDistance(playerOrBoss, *target));
		v.targetIsRooted = target->getEffectController()->isAbnormalSet(AbnormalState::ROOT);
		v.targetIsSilenced = target->getEffectController()->isAbnormalSet(AbnormalState::SILENCE);
		v.targetIsBound = target->getEffectController()->isAbnormalSet(AbnormalState::BIND);
		v.targetIsStunned = target->getEffectController()->isAbnormalSet(AbnormalState::ANY_STUN);
		v.targetIsAetherhold = target->getEffectController()->isAbnormalSet(AbnormalState::OPENAERIAL);
		v.targetIsShielded = target->getEffectController()->isUnderNormalShield();
		v.targetBuffCount = 0;
		v.targetDebuffCount = 0;
		for (runtime::Ptr<skillengine::model::Effect> e : target->getEffectController()->getAbnormalEffects())
			if ((e->getSkillTemplate() != nullptr && e->getSkillTemplate()->getSubType() == skillengine::model::SkillSubType::BUFF) || // buff skills
				(e->getSkill() != nullptr && e->getSkill()->getItemObjectId() != 0)) // buff items
				v.targetBuffCount++;
			else // debuffs
				v.targetDebuffCount++;
	} else {
		v.targetBuffCount = v.targetDebuffCount = -1;
		v.targetHPpercentage = v.distance = v.targetMPpercentage = -1;
		v.targetFocusesPlayer = v.targetIsRooted = v.targetIsSilenced = v.targetIsBound = v.targetIsStunned = v.targetIsAetherhold =
			v.targetIsShielded = false;
	}
	return v;
}

// Java PlayerModelEntry.java:40-102
PlayerModelEntry::PlayerModelEntry(model::gameobjects::Creature& playerOrBoss, int32_t skillID, runtime::Ptr<model::gameobjects::Creature> target)
	: PlayerModelEntry(liveValues(playerOrBoss, skillID, target), Persistable_PersistentState::NEW) {
}

runtime::Ref<PlayerModelEntry> PlayerModelEntry::create(model::gameobjects::Creature& playerOrBoss, int32_t skillID,
	runtime::Ptr<model::gameobjects::Creature> target) {
	return runtime::makeRef<PlayerModelEntry>(playerOrBoss, skillID, target);
}

// Java PlayerModelEntry.java:105-136
PlayerModelEntry::PlayerModelEntry(int32_t playerID, commons::database::Timestamp timestamp, int32_t skillID, int32_t playerClassID,
	float playerHPpercentage, float playerMPpercentage, bool playerIsRooted, bool playerIsSilenced, bool playerIsBound, bool playerIsStunned,
	bool playerIsAetherhold, int32_t playerBuffCount, int32_t playerDebuffCount, bool playerIsShielded, float targetHPpercentage,
	float targetMPpercentage, bool targetFocusesPlayer, float distance, bool targetIsRooted, bool targetIsSilenced, bool targetIsBound,
	bool targetIsStunned, bool targetIsAetherhold, int32_t targetBuffCount, int32_t targetDebuffCount, bool targetIsShielded)
	: PlayerModelEntry(Values{timestamp, skillID, playerID, playerClassID, playerHPpercentage, playerMPpercentage, playerIsRooted, playerIsSilenced,
						   playerIsBound, playerIsStunned, playerIsAetherhold, playerBuffCount, playerDebuffCount, playerIsShielded, targetHPpercentage,
						   targetMPpercentage, targetFocusesPlayer, distance, targetIsRooted, targetIsSilenced, targetIsBound, targetIsStunned,
						   targetIsAetherhold, targetBuffCount, targetDebuffCount, targetIsShielded},
		  Persistable_PersistentState::UPDATED) {
}

runtime::Ref<PlayerModelEntry> PlayerModelEntry::create(int32_t playerID, commons::database::Timestamp timestamp, int32_t skillID,
	int32_t playerClassID, float playerHPpercentage, float playerMPpercentage, bool playerIsRooted, bool playerIsSilenced, bool playerIsBound,
	bool playerIsStunned, bool playerIsAetherhold, int32_t playerBuffCount, int32_t playerDebuffCount, bool playerIsShielded,
	float targetHPpercentage, float targetMPpercentage, bool targetFocusesPlayer, float distance, bool targetIsRooted, bool targetIsSilenced,
	bool targetIsBound, bool targetIsStunned, bool targetIsAetherhold, int32_t targetBuffCount, int32_t targetDebuffCount, bool targetIsShielded) {
	return runtime::makeRef<PlayerModelEntry>(playerID, timestamp, skillID, playerClassID, playerHPpercentage, playerMPpercentage, playerIsRooted,
		playerIsSilenced, playerIsBound, playerIsStunned, playerIsAetherhold, playerBuffCount, playerDebuffCount, playerIsShielded,
		targetHPpercentage, targetMPpercentage, targetFocusesPlayer, distance, targetIsRooted, targetIsSilenced, targetIsBound, targetIsStunned,
		targetIsAetherhold, targetBuffCount, targetDebuffCount, targetIsShielded);
}

// Java PlayerModelEntry.java:138-166
std::vector<double> PlayerModelEntry::toStateInputArray(const std::vector<int32_t>& skillSet, int32_t previousSkillID) const {
	std::vector<double> input;
	input.push_back(static_cast<double>(playerHPpercentage));
	input.push_back(static_cast<double>(playerMPpercentage));
	input.push_back(static_cast<double>(playerIsRooted ? 1 : 0));
	input.push_back(static_cast<double>(playerIsSilenced ? 1 : 0));
	input.push_back(static_cast<double>(playerIsBound ? 1 : 0));
	input.push_back(static_cast<double>(playerIsStunned ? 1 : 0));
	input.push_back(static_cast<double>(playerIsAetherhold ? 1 : 0));
	input.push_back(static_cast<double>(playerBuffCount));
	input.push_back(static_cast<double>(playerDebuffCount));
	input.push_back(static_cast<double>(playerIsShielded ? 1 : 0));
	input.push_back(static_cast<double>(targetHPpercentage));
	input.push_back(static_cast<double>(targetMPpercentage));
	input.push_back(static_cast<double>(targetFocusesPlayer ? 1 : 0));
	input.push_back(static_cast<double>(distance));
	input.push_back(static_cast<double>(targetIsRooted ? 1 : 0));
	input.push_back(static_cast<double>(targetIsSilenced ? 1 : 0));
	input.push_back(static_cast<double>(targetIsBound ? 1 : 0));
	input.push_back(static_cast<double>(targetIsStunned ? 1 : 0));
	input.push_back(static_cast<double>(targetIsAetherhold ? 1 : 0));
	input.push_back(static_cast<double>(targetBuffCount));
	input.push_back(static_cast<double>(targetDebuffCount));
	input.push_back(static_cast<double>(targetIsShielded ? 1 : 0));
	for (int32_t skill : skillSet)
		input.push_back(static_cast<double>(skill == previousSkillID ? 1 : 0));
	return input;
}

// Java PlayerModelEntry.java:168-170
std::vector<double> PlayerModelEntry::toActionOutputArray(const std::vector<int32_t>& skillSet) const {
	std::vector<double> output;
	for (int32_t skillId : skillSet)
		output.push_back(skillId == this->skillID ? 1 : 0);
	return output;
}

PlayerModelEntry::~PlayerModelEntry() = default;

} // namespace aion::gameserver::custom::instance::neuralnetwork
