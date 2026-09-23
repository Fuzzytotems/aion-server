#pragma once

#include "aion/gameserver/skillengine/properties/Properties.xml.h"

#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::properties {

/**
 * Java com.aionemu.gameserver.skillengine.properties.Properties: the target rules of a skill template (first target, first target range,
 * target range, relation, status, species and maximum count) and the validation that applies them to a cast.
 * <p>
 * C++ notes (declarations of m5b2-plan.md S-03; the bodies are the cast lane's):
 * - validate and endCastValidate run the seven `*Property::set` steps in Java's order; the skill's effected list is filtered in place.
 * - The effected list is the skill's `runtime::ArrayList<Ref<Creature>>` field (Skill.h), so validateEffectedList takes and ValidationResult
 *   keeps a reference to it, as Java keeps the list object (hub-headers.md §7.1: a list the callee fills for a shared field is passed as the
 *   shim by reference). SkillEngine::applyEffectsDirectly passes a local list of the same type.
 * - `firstTarget` parameters are nullable (`Skill.getFirstTarget()`, MaxCountProperty checks the result's first target for null).
 * - isAddWeaponRange and the other getters are generated (Properties.xml.inc).
 *
 * @author ATracer
 */
class Properties : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/skillengine/properties/Properties.xml.inc"
public:
	class ValidationResult;

	bool validate(model::Skill& skill, CastState castState) const;

	bool endCastValidate(model::Skill& skill) const;

private:
	bool validateEffectedList(model::Skill& skill) const;

public:
	ValidationResult validateEffectedList(runtime::ArrayList<runtime::Ref<gameserver::model::gameobjects::Creature>>& targets,
		runtime::Ptr<gameserver::model::gameobjects::Creature> firstTarget, gameserver::model::gameobjects::Creature& effector,
		const model::SkillTemplate* skillTemplate, float x, float y, float z) const;

	/**
	 * Java Properties.ValidationResult: the targets and the first target one validation filters.
	 * <p>
	 * C++: a confined value (fieldmap K5) that lives for one validation, members public (hub-headers.md §9.3). `targets` is the caller's list
	 * (Java's final field holds the list object the caller passed); `firstTarget` is nullable.
	 */
	class ValidationResult {
	public:
		// Java's final List field holds the caller's list (the skill's effectedList, which the property setters filter in place and
		// Skill.endCast reads afterwards); fieldmap prints a std::vector copy, which would lose the filtering (header request m5b2-fm-1).
		// m5b2-fm-1 (fieldmap.toml): a reference to the caller's list, as Java's final field holds the list object itself
		runtime::ArrayList<runtime::Ref<gameserver::model::gameobjects::Creature>>& targets;
		runtime::Ptr<gameserver::model::gameobjects::Creature> firstTarget;
		bool valid = false;

		ValidationResult(runtime::ArrayList<runtime::Ref<gameserver::model::gameobjects::Creature>>& targets,
			runtime::Ptr<gameserver::model::gameobjects::Creature> firstTarget);

		runtime::ArrayList<runtime::Ref<gameserver::model::gameobjects::Creature>>& getTargets() { return targets; }

		runtime::Ptr<gameserver::model::gameobjects::Creature> getFirstTarget() const { return firstTarget; }

		void setFirstTarget(runtime::Ptr<gameserver::model::gameobjects::Creature> value) { firstTarget = value; }

		bool isValid() const { return valid; }
	};
};

} // namespace aion::gameserver::skillengine::properties
