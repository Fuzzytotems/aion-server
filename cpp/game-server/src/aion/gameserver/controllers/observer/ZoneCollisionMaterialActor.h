#pragma once

#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/controllers/observer/AbstractMaterialSkillActor.h"
#include "aion/gameserver/controllers/observer/fwd.h"
#include "aion/gameserver/geoEngine/collision/fwd.h"
#include "aion/gameserver/geoEngine/scene/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/templates/materials/fwd.h"

namespace aion::gameserver::controllers::observer {

/**
 * Applies the material skills of a zone geometry while the creature touches (or passed into) it.
 * <p>
 * RefCounted AbstractMaterialSkillActor (fieldmap K4), created with create() by ZoneInstance. The matching skills list is stored (§7.1: by
 * value).
 *
 * @author Rolandas
 */
class ZoneCollisionMaterialActor : public AbstractMaterialSkillActor {
	AION_MAKE_REF_FRIEND
protected:
	ZoneCollisionMaterialActor(model::gameobjects::Creature& creature, runtime::Ptr<geoEngine::scene::Spatial> geometry,
		std::vector<const model::templates::materials::MaterialSkill*> matchingSkills, CheckType checkType);
	~ZoneCollisionMaterialActor() override;

public:
	/** Java: new ZoneCollisionMaterialActor(creature, geometry, matchingSkills, checkType) */
	static runtime::Ref<ZoneCollisionMaterialActor> create(model::gameobjects::Creature& creature, runtime::Ptr<geoEngine::scene::Spatial> geometry,
		std::vector<const model::templates::materials::MaterialSkill*> matchingSkills, CheckType checkType);

	void onMoved(geoEngine::collision::CollisionResults& collisionResults) override;
};

} // namespace aion::gameserver::controllers::observer
