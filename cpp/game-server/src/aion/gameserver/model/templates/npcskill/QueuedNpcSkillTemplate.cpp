#include "aion/gameserver/model/templates/npcskill/QueuedNpcSkillTemplate.h"

namespace aion::gameserver::model::templates::npcskill {

QueuedNpcSkillTemplate::QueuedNpcSkillTemplate(int32_t idValue, int32_t lvValue) : QueuedNpcSkillTemplate(idValue, lvValue, -1) {}

QueuedNpcSkillTemplate::QueuedNpcSkillTemplate(int32_t idValue, int32_t lvValue, int32_t nextSkillTimeValue)
    : QueuedNpcSkillTemplate(idValue, lvValue, nextSkillTimeValue, NpcSkillTargetAttribute::MOST_HATED) {}

QueuedNpcSkillTemplate::QueuedNpcSkillTemplate(int32_t idValue, int32_t lvValue, int32_t nextSkillTimeValue,
                                               NpcSkillTargetAttribute npcSkillTargetAttribute) {
	this->id = idValue;
	this->lv = lvValue;
	this->prob = 100;
	this->nextSkillTime = nextSkillTimeValue;
	this->target = npcSkillTargetAttribute;
}

} // namespace aion::gameserver::model::templates::npcskill
