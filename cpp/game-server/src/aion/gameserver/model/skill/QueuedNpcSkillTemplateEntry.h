#pragma once

#include <memory>

#include "aion/gameserver/model/skill/NpcSkillTemplateEntry.h"
#include "aion/gameserver/model/templates/npcskill/QueuedNpcSkillTemplate.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::model::skill {

/**
 * C++ only: Java's `new NpcSkillTemplateEntry(new QueuedNpcSkillTemplate(...))` (Npc.queueSkill). NpcSkillTemplateEntry points at its
 * template, which is static data for the npc skill lists; a queued template is created at run time, so this entry owns it (Java: the entry
 * holds the only reference). Behaviour is NpcSkillTemplateEntry's.
 */
class QueuedNpcSkillTemplateEntry final : public NpcSkillTemplateEntry {
	AION_MAKE_REF_FRIEND
private:
	// confined: the queued template is built once and only read (const), owned by this entry alone, as Java's entry holds its only reference
	const std::unique_ptr<const templates::npcskill::QueuedNpcSkillTemplate> queuedTemplate;

	explicit QueuedNpcSkillTemplateEntry(std::unique_ptr<const templates::npcskill::QueuedNpcSkillTemplate> templateValue);

protected:
	~QueuedNpcSkillTemplateEntry() override;

public:
	static runtime::Ref<QueuedNpcSkillTemplateEntry> create(std::unique_ptr<const templates::npcskill::QueuedNpcSkillTemplate> templateValue);
};

} // namespace aion::gameserver::model::skill
