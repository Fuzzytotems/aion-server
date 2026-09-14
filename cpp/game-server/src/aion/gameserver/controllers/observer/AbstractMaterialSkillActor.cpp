#include "aion/gameserver/controllers/observer/AbstractMaterialSkillActor.h"

#include <utility>

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/geoEngine/scene/Spatial.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/Creature.h"

namespace aion::gameserver::controllers::observer {

AbstractMaterialSkillActor::AbstractMaterialSkillActor(model::gameobjects::Creature& creatureValue,
	runtime::Ptr<geoEngine::scene::Spatial> geometryValue, int8_t intentionsValue, CheckType checkType, model::TaskId taskIdValue,
	std::vector<const model::templates::materials::MaterialSkill*> skillsValue)
	: AbstractCollisionObserver(creatureValue, geometryValue, intentionsValue, checkType), taskId(taskIdValue) {
	runtime::Ref<runtime::RcArrayList<const model::templates::materials::MaterialSkill*>> list =
		runtime::RcArrayList<const model::templates::materials::MaterialSkill*>::create(AION_LOCK_CLASS(AbstractMaterialSkillActor::skills));
	for (const model::templates::materials::MaterialSkill* skill : skillsValue)
		list->add(skill);
	skills.set(std::move(list)); // Java: this.skills = skills
}

AbstractMaterialSkillActor::~AbstractMaterialSkillActor() = default;

// callbacks: MaterialSkillTask (Java inner Runnable, scheduleAtFixedRate, stored in task and the controller's task map)
void AbstractMaterialSkillActor::act() {
	AION_UNPORTED();
}

void AbstractMaterialSkillActor::abort() {
	AION_UNPORTED();
}

void AbstractMaterialSkillActor::died(model::gameobjects::Creature& value) {
	AION_UNPORTED();
}

const model::templates::materials::MaterialSkill* AbstractMaterialSkillActor::findFirstSkillWithMatchingCondition() {
	AION_UNPORTED();
}

bool AbstractMaterialSkillActor::matchActConditions(const model::templates::materials::MaterialSkill* skill) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::controllers::observer
