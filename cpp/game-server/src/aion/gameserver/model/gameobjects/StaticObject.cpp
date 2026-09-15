#include "aion/gameserver/model/gameobjects/StaticObject.h"

namespace aion::gameserver::model::gameobjects {

// StaticObject(CreateKey, std::unique_ptr<controllers::StaticObjectController>, SpawnTemplate&, const VisibleObjectTemplate*) is defined once
// controllers/StaticObjectController.h exists (P4-11b): the controller conversion and controller.setOwner(this) need the complete class.

StaticObject::~StaticObject() = default;

} // namespace aion::gameserver::model::gameobjects
