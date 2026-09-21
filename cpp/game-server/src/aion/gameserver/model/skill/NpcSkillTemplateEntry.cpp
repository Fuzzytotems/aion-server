#include "aion/gameserver/model/skill/NpcSkillTemplateEntry.h"

#include <string>

#include "aion/commons/utils/Rnd.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/stats/container/NpcGameStats.h"
#include "aion/gameserver/model/templates/npcskill/NpcSkillConditionTemplate.h"
#include "aion/gameserver/model/templates/npcskill/NpcSkillTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::skill {

using templates::npcskill::ConjunctionType;
using templates::npcskill::NpcSkillCondition;

NpcSkillTemplateEntry::NpcSkillTemplateEntry(const templates::npcskill::NpcSkillTemplate& templateValue)
	: NpcSkillEntry(templateValue.getSkillId(), templateValue.getSkillLevel()), template_(&templateValue) {
}

NpcSkillTemplateEntry::~NpcSkillTemplateEntry() = default;

runtime::Ref<NpcSkillTemplateEntry> NpcSkillTemplateEntry::create(const templates::npcskill::NpcSkillTemplate& templateValue) {
	return runtime::makeRef<NpcSkillTemplateEntry>(templateValue);
}

bool NpcSkillTemplateEntry::isReady(int32_t hpPercentage, int64_t fightingTimeInMSec) {
	if (hasCooldown() || !chanceReady())
		return false;

	switch (template_->getConjunctionType()) {
		case ConjunctionType::XOR:
			return (hpReady(hpPercentage) && !timeReady(fightingTimeInMSec)) || (!hpReady(hpPercentage) && timeReady(fightingTimeInMSec));
		case ConjunctionType::OR:
			return hpReady(hpPercentage) || timeReady(fightingTimeInMSec);
		case ConjunctionType::AND:
			return hpReady(hpPercentage) && timeReady(fightingTimeInMSec);
	}
	// Java: the switch expression over the enum is exhaustive (an unknown constant is an IncompatibleClassChangeError)
	throw runtime::IllegalStateException("unknown conjunction type of skill " + std::to_string(getSkillId()));
}

bool NpcSkillTemplateEntry::chanceReady() {
	return commons::utils::Rnd::chance() < static_cast<float>(template_->getProbability());
}

bool NpcSkillTemplateEntry::hpReady(int32_t hpPercentage) {
	if (template_->getMaxhp() == 100 && template_->getMinhp() == 0) // it's not about hp
		return true;
	else if (template_->getMaxhp() >= hpPercentage && template_->getMinhp() <= hpPercentage) // in hp range
		return true;
	else
		return false;
}

bool NpcSkillTemplateEntry::timeReady(int64_t elapsedFightTime) {
	int64_t minTime = template_->getMinTime();
	int64_t maxTime = template_->getMaxTime();
	return (maxTime == 0 && minTime == 0) || (maxTime == 0 && minTime <= elapsedFightTime)
		|| (maxTime >= elapsedFightTime && minTime <= elapsedFightTime);
}

bool NpcSkillTemplateEntry::hasCooldown() {
	return template_->getCooldown() > (commons::utils::currentTimeMillis() - lastTimeUsed.get());
}

bool NpcSkillTemplateEntry::hasPostSpawnCondition() {
	return template_->isPostSpawn();
}

int32_t NpcSkillTemplateEntry::getPriority() {
	return template_->getPriority();
}

bool NpcSkillTemplateEntry::conditionReady(gameobjects::Creature& creature) {
	AION_UNPORTED();
}

const templates::npcskill::NpcSkillConditionTemplate* NpcSkillTemplateEntry::getConditionTemplate() {
	return template_->getConditionTemplate();
}

bool NpcSkillTemplateEntry::hasCondition() {
	return getConditionTemplate() != nullptr && getConditionTemplate()->getCondType() != NpcSkillCondition::NONE;
}

int32_t NpcSkillTemplateEntry::getNextSkillTime() {
	return template_->getNextSkillTime();
}

bool NpcSkillTemplateEntry::hasChain() {
	return template_->getNextChainId() > 0;
}

int32_t NpcSkillTemplateEntry::getNextChainId() {
	return template_->getNextChainId();
}

int32_t NpcSkillTemplateEntry::getChainId() {
	return template_->getChainId();
}

const templates::npcskill::NpcSkillTemplate* NpcSkillTemplateEntry::getTemplate() {
	return template_;
}

bool NpcSkillTemplateEntry::canUseNextChain(gameobjects::Npc& owner) {
	// Java: owner != null && ... - the parameter is a reference here, so the null check is the caller's (docs/deviations/P5-02.md)
	return (commons::utils::currentTimeMillis() - owner.getGameStats()->getLastSkillTime()) < template_->getMaxChainTime();
}

void NpcSkillTemplateEntry::fireOnEndCastEvents(gameobjects::Npc& npc) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::skill
