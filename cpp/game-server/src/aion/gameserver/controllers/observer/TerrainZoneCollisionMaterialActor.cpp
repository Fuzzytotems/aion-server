#include "aion/gameserver/controllers/observer/TerrainZoneCollisionMaterialActor.h"

#include <vector>

#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/MaterialData.h"
#include "aion/gameserver/geoEngine/collision/CollisionIntention.h"
#include "aion/gameserver/geoEngine/collision/CollisionIntentionInfo.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/templates/materials/MaterialSkill.h"
#include "aion/gameserver/model/templates/materials/MaterialTargetInfo.h"
#include "aion/gameserver/model/templates/materials/MaterialTemplate.h"
#include "aion/gameserver/runtime/sync/Monitor.h"
#include "aion/gameserver/world/geo/GeoService.h"

namespace aion::gameserver::controllers::observer {

using model::templates::materials::MaterialSkill;
using runtime::Ptr;
using runtime::Ref;

TerrainZoneCollisionMaterialActor::TerrainZoneCollisionMaterialActor(model::gameobjects::Creature& creatureValue)
	: AbstractMaterialSkillActor(creatureValue, nullptr, geoEngine::collision::getId(geoEngine::collision::CollisionIntention::MATERIAL),
		  CheckType::TOUCH, model::TaskId::TERRAIN_MATERIAL_ACTION, {}) {
}

TerrainZoneCollisionMaterialActor::~TerrainZoneCollisionMaterialActor() = default;

runtime::Ref<TerrainZoneCollisionMaterialActor> TerrainZoneCollisionMaterialActor::create(model::gameobjects::Creature& creatureValue) {
	return runtime::makeRef<TerrainZoneCollisionMaterialActor>(creatureValue);
}

void TerrainZoneCollisionMaterialActor::onMoved(geoEngine::collision::CollisionResults& collisionResults) {
	// Java: empty body
}

void TerrainZoneCollisionMaterialActor::moved() {
	Ptr<model::gameobjects::Creature> observed = creature.get();
	if (world::geo::GeoService::getInstance().worldHasTerrainMaterials(observed->getWorldId())) {
		int32_t matId = world::geo::GeoService::getInstance().getTerrainMaterialAt(observed->getWorldId(), observed->getX(), observed->getY(),
			observed->getZ(), observed->getInstanceId());
		if (matId != lastMatId.get() || !isTouched.get()) {
			lastMatId = matId;
			isTouched = true;
			const model::templates::materials::MaterialTemplate* template_ = matId == 0 ? nullptr : dataholders::DataManager::MATERIAL_DATA->getTemplate(matId);
			if (template_ != nullptr) {
				Ref<runtime::RcArrayList<const MaterialSkill*>> matchingSkills =
					runtime::RcArrayList<const MaterialSkill*>::create(AION_LOCK_CLASS(AbstractMaterialSkillActor::skills));
				for (const MaterialSkill& skill : template_->getSkills()) {
					if (model::templates::materials::matches(skill.getTarget(), *observed))
						matchingSkills->add(&skill);
				}
				if (!matchingSkills->isEmpty()) {
					skills = std::move(matchingSkills);
					act();
					return;
				}
			}
		}
	}
	if (!skills->isEmpty()) {
		Ptr<runtime::RcArrayList<const MaterialSkill*>> currentSkills = skills.get();
		SYNCHRONIZED(*currentSkills) {
			currentSkills->clear();
			isTouched = false;
			abort();
		}
	}
}

} // namespace aion::gameserver::controllers::observer
