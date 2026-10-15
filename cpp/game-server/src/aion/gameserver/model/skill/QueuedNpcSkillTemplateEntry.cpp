#include "aion/gameserver/model/skill/QueuedNpcSkillTemplateEntry.h"

#include <utility>

namespace aion::gameserver::model::skill {

// the base reads the template through its address, which moving the unique_ptr into the member keeps
QueuedNpcSkillTemplateEntry::QueuedNpcSkillTemplateEntry(std::unique_ptr<const templates::npcskill::QueuedNpcSkillTemplate> templateValue)
	: NpcSkillTemplateEntry(*templateValue), queuedTemplate(std::move(templateValue)) {
}

QueuedNpcSkillTemplateEntry::~QueuedNpcSkillTemplateEntry() = default;

runtime::Ref<QueuedNpcSkillTemplateEntry> QueuedNpcSkillTemplateEntry::create(
	std::unique_ptr<const templates::npcskill::QueuedNpcSkillTemplate> templateValue) {
	return runtime::makeRef<QueuedNpcSkillTemplateEntry>(std::move(templateValue));
}

} // namespace aion::gameserver::model::skill
