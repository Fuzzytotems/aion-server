#include "aion/gameserver/world/zone/handler/MaterialZoneHandler.h"

#include <string>
#include <vector>

#include "aion/gameserver/configs/main/GeoDataConfig.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/observer/AbstractCollisionObserver.h"
#include "aion/gameserver/controllers/observer/AbstractMaterialSkillActor.h"
#include "aion/gameserver/controllers/observer/ZoneCollisionMaterialActor.h"
#include "aion/gameserver/geoEngine/scene/Spatial.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/materials/MaterialSkill.h"
#include "aion/gameserver/model/templates/materials/MaterialTargetInfo.h"
#include "aion/gameserver/model/templates/materials/MaterialTemplate.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/world/zone/ZoneInstance.h"

namespace aion::gameserver::world::zone::handler {

namespace {

/** Java constructor body: BU_AB_DARKSP meshes belong to the Asmodians, BU_AB_LIGHTSP meshes to the Elyos */
model::Race ownerRaceOf(geoEngine::scene::Spatial& geometry) {
	std::string name = geometry.getName();
	if (name.starts_with("BU_AB_DARKSP"))
		return model::Race::ASMODIANS;
	else if (name.starts_with("BU_AB_LIGHTSP"))
		return model::Race::ELYOS;
	return model::Race::NONE;
}

} // namespace

MaterialZoneHandler::MaterialZoneHandler(geoEngine::scene::Spatial& geometryValue, const model::templates::materials::MaterialTemplate* templateValue)
	: geometry(geometryValue), template_(templateValue), ownerRace(ownerRaceOf(geometryValue)) {
}

MaterialZoneHandler::~MaterialZoneHandler() = default;

runtime::Ref<MaterialZoneHandler> MaterialZoneHandler::create(geoEngine::scene::Spatial& geometryValue,
	const model::templates::materials::MaterialTemplate* templateValue) {
	return runtime::makeRef<MaterialZoneHandler>(geometryValue, templateValue);
}

void MaterialZoneHandler::onEnterZone(model::gameobjects::Creature& creature, ZoneInstance& zone) {
	if (ownerRace == creature.getRace())
		return;
	std::vector<const model::templates::materials::MaterialSkill*> matchingSkills;
	for (const model::templates::materials::MaterialSkill& skill : template_->getSkills()) {
		if (matches(skill.getTarget(), creature))
			matchingSkills.push_back(&skill);
	}
	if (matchingSkills.empty())
		return;
	// Teminon/Primum Landing shield 14 & 15, abyss core 16
	controllers::observer::AbstractCollisionObserver::CheckType checkType = geometry->getMaterialId() >= 14 && geometry->getMaterialId() <= 16
		? controllers::observer::AbstractCollisionObserver::CheckType::PASS
		: controllers::observer::AbstractCollisionObserver::CheckType::TOUCH;
	runtime::Ref<controllers::observer::ZoneCollisionMaterialActor> actor =
		controllers::observer::ZoneCollisionMaterialActor::create(creature, geometry, std::move(matchingSkills), checkType);
	creature.getObserveController()->addObserver(*actor);
	observed.put(creature.getObjectId(), actor);
	if (configs::main::GeoDataConfig::GEO_MATERIALS_SHOWDETAILS.load()) {
		if (auto* player = dynamic_cast<model::gameobjects::player::Player*>(&creature); player != nullptr && player->isStaff())
			utils::PacketSendUtility::sendMessage(*player, "Entered material zone " + geometry->getName());
	}
	actor->moved();
}

void MaterialZoneHandler::onLeaveZone(model::gameobjects::Creature& creature, ZoneInstance& zone) {
	runtime::Ptr<controllers::observer::AbstractMaterialSkillActor> actor = observed.remove(creature.getObjectId());
	if (actor) {
		creature.getObserveController()->removeObserver(*actor);
		actor->abort();
	}
	if (configs::main::GeoDataConfig::GEO_MATERIALS_SHOWDETAILS.load()) {
		if (auto* player = dynamic_cast<model::gameobjects::player::Player*>(&creature); player != nullptr && player->isStaff())
			utils::PacketSendUtility::sendMessage(*player, "Left material zone " + geometry->getName());
	}
}

} // namespace aion::gameserver::world::zone::handler
