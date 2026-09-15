#include "aion/gameserver/controllers/observer/ZoneCollisionMaterialActor.h"

#include <optional>
#include <string>
#include <utility>

#include "aion/gameserver/configs/main/GeoDataConfig.h"
#include "aion/gameserver/geoEngine/collision/CollisionIntention.h"
#include "aion/gameserver/geoEngine/collision/CollisionIntentionInfo.h"
#include "aion/gameserver/geoEngine/collision/CollisionResult.h"
#include "aion/gameserver/geoEngine/collision/CollisionResults.h"
#include "aion/gameserver/geoEngine/scene/Geometry.h"
#include "aion/gameserver/geoEngine/scene/Spatial.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::controllers::observer {

using model::gameobjects::player::Player;
using runtime::Ptr;

ZoneCollisionMaterialActor::ZoneCollisionMaterialActor(model::gameobjects::Creature& creatureValue,
	runtime::Ptr<geoEngine::scene::Spatial> geometryValue, std::vector<const model::templates::materials::MaterialSkill*> matchingSkills,
	CheckType checkType)
	: AbstractMaterialSkillActor(creatureValue, geometryValue, geoEngine::collision::getId(geoEngine::collision::CollisionIntention::MATERIAL),
		  checkType, model::TaskId::ZONE_MATERIAL_ACTION, std::move(matchingSkills)) {
}

ZoneCollisionMaterialActor::~ZoneCollisionMaterialActor() = default;

runtime::Ref<ZoneCollisionMaterialActor> ZoneCollisionMaterialActor::create(model::gameobjects::Creature& creatureValue,
	runtime::Ptr<geoEngine::scene::Spatial> geometryValue, std::vector<const model::templates::materials::MaterialSkill*> matchingSkills,
	CheckType checkType) {
	return runtime::makeRef<ZoneCollisionMaterialActor>(creatureValue, geometryValue, std::move(matchingSkills), checkType);
}

void ZoneCollisionMaterialActor::onMoved(geoEngine::collision::CollisionResults& collisionResults) {
	bool oldTouched = isTouched.get();
	isTouched = collisionResults.size() > 0;
	if (oldTouched != isTouched.get()) {
		if (isTouched.get())
			act();
		else
			abort();
		if (Ptr<Player> player = runtime::as<Player>(creature.get()); configs::main::GeoDataConfig::GEO_MATERIALS_SHOWDETAILS && player && player->isStaff()) {
			std::string name = collisionResults.size() > 0 ? collisionResults.getClosestCollision().value().getGeometry()->getName() : geometry->getName();
			utils::PacketSendUtility::sendMessage(*player, (isTouched.get() ? "Touched " : "Untouched ") + name);
		}
	}
}

} // namespace aion::gameserver::controllers::observer
