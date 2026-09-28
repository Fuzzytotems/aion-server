#include "aion/gameserver/model/gameobjects/StaticObject.h"

#include <utility>

#include "aion/gameserver/controllers/StaticObjectController.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::model::gameobjects {

StaticObject::StaticObject(CreateKey key, std::unique_ptr<controllers::StaticObjectController> controller,
	templates::spawns::SpawnTemplate& spawnTemplate, const templates::VisibleObjectTemplate* objectTemplate)
	: VisibleObject(key, utils::idfactory::IDFactory::getInstance().nextId(), std::move(controller), spawnTemplate, objectTemplate,
		  world::WorldPosition::create(spawnTemplate.getWorldId()), true) {
	// Java: controller.setOwner(this) (binds the late-bound part before publication; no virtual call on the owner)
	getController().setOwner(*this);
}

StaticObject::~StaticObject() = default;

} // namespace aion::gameserver::model::gameobjects
