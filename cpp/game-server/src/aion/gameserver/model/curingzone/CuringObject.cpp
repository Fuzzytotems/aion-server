#include "aion/gameserver/model/curingzone/CuringObject.h"

#include <memory>

#include "aion/gameserver/controllers/VisibleObjectController.h"
#include "aion/gameserver/model/templates/VisibleObjectTemplate.h"
#include "aion/gameserver/model/templates/curingzones/CuringTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/knownlist/NpcKnownList.h"

namespace aion::gameserver::model::curingzone {

namespace {

// anonymous VisibleObjectController at CuringObject.java:19 (model.curingzone.CuringObject$1); argument 2 of super(); storage: stored in
// VisibleObject. Java never binds its owner (no setOwner call), so neither does the port.
class CuringObjectController final : public controllers::VisibleObjectController {};

/** Java super(...) argument: World.getInstance().createPosition(template.getMapId(), x, y, z, (byte) 0, instanceId) (NPE for a null template) */
runtime::Ref<world::WorldPosition> positionOf(const templates::curingzones::CuringTemplate* template_, int32_t instanceId) {
	if (template_ == nullptr)
		throw runtime::NullPointerException("template");
	return world::World::getInstance().createPosition(template_->getMapId(), template_->getX(), template_->getY(), template_->getZ(), int8_t{0},
		instanceId);
}

} // namespace

CuringObject::CuringObject(CreateKey key, const templates::curingzones::CuringTemplate* value, int32_t instanceId)
	: gameobjects::VisibleObject(key, utils::idfactory::IDFactory::getInstance().nextId(), std::make_unique<CuringObjectController>(), nullptr,
		  nullptr, positionOf(value, instanceId), true),
	  template_(value), range(value->getRange()) {
	setKnownlist(std::make_unique<world::knownlist::NpcKnownList>(*this));
}

std::string CuringObject::getName() {
	return "";
}

void CuringObject::spawn() {
	world::World& w = world::World::getInstance();
	w.storeObject(*this);
	w.spawn(*this);
}

CuringObject::~CuringObject() = default;

} // namespace aion::gameserver::model::curingzone
