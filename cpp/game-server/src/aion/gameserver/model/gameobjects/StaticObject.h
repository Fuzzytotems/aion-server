#pragma once

#include <memory>

#include "aion/gameserver/controllers/fwd.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/templates/fwd.h"
#include "aion/gameserver/model/templates/spawns/fwd.h"

namespace aion::gameserver::model::gameobjects {

/**
 * A static world object (doors, ...). A visible object: `VisibleObject::create<StaticObject>(controller, spawnTemplate, objectTemplate)`
 * (§10.1): the constructor takes its object id from IDFactory, a new WorldPosition of the spawn's world and binds the controller.
 *
 * @author ATracer
 */
class StaticObject : public VisibleObject {
	AION_MAKE_REF_FRIEND
protected:
	/** Java: super(IDFactory.nextId(), controller, spawnTemplate, objectTemplate, new WorldPosition(spawnTemplate.getWorldId()), true) */
	StaticObject(CreateKey key, std::unique_ptr<controllers::StaticObjectController> controller, templates::spawns::SpawnTemplate& spawnTemplate,
		const templates::VisibleObjectTemplate* objectTemplate);
	~StaticObject() override;
};

} // namespace aion::gameserver::model::gameobjects
