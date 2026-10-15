#pragma once

#include <memory>

#include "aion/gameserver/controllers/fwd.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/templates/gather/fwd.h"
#include "aion/gameserver/model/templates/spawns/fwd.h"

namespace aion::gameserver::model::gameobjects {

/**
 * A gatherable resource (ore, plant, ...). A visible object: `VisibleObject::create<Gatherable>(spawnTemplate, controller)` (§10.1): the
 * constructor takes its object id from IDFactory and its template from DataManager.GATHERABLE_DATA, binds the controller and gets a
 * PlayerAwareKnownList.
 *
 * @author ATracer
 */
class Gatherable : public VisibleObject {
	AION_MAKE_REF_FRIEND
protected:
	Gatherable(CreateKey key, templates::spawns::SpawnTemplate& spawnTemplate, std::unique_ptr<controllers::GatherableController> controller);
	~Gatherable() override;

public:
	/** Narrows VisibleObject::getObjectTemplate (Java cast-only override) */
	const templates::gather::GatherableTemplate* getObjectTemplate() const;

	/** Narrows VisibleObject::getController (Java cast-only override) */
	controllers::GatherableController& getController() const;
};

} // namespace aion::gameserver::model::gameobjects
