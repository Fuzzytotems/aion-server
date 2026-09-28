#pragma once

#include <cstdint>

#include "aion/gameserver/model/templates/npcskill/NpcSkillTargetAttribute.h"
#include "aion/gameserver/model/templates/npcskill/NpcSkillTemplate.h"

namespace aion::gameserver::model::templates::npcskill {

/**
 * Java com.aionemu.gameserver.model.templates.npcskill.QueuedNpcSkillTemplate: a skill template created at run time for Npc.queueSkill.
 * <p>
 * C++: K5 (fieldmap); the NpcSkillTemplateEntry port decides who owns the object Java hands to `new NpcSkillTemplateEntry(...)`.
 *
 * @author Yeats
 */
class QueuedNpcSkillTemplate : public NpcSkillTemplate {
public:
	QueuedNpcSkillTemplate(int32_t id, int32_t lv);

	QueuedNpcSkillTemplate(int32_t id, int32_t lv, int32_t nextSkillTime);

	QueuedNpcSkillTemplate(int32_t id, int32_t lv, int32_t nextSkillTime, NpcSkillTargetAttribute npcSkillTargetAttribute);
};

} // namespace aion::gameserver::model::templates::npcskill
