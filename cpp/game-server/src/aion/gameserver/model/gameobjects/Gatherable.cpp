#include "aion/gameserver/model/gameobjects/Gatherable.h"

#include "aion/gameserver/controllers/GatherableController.h"
#include "aion/gameserver/model/templates/gather/GatherableTemplate.h"

namespace aion::gameserver::model::gameobjects {

// Gatherable(CreateKey, SpawnTemplate&, std::unique_ptr<controllers::GatherableController>) is defined once the GatherableController destructor
// can be (it needs skillengine/task/GatheringTask.h, P5-02; see the class comment). Java: super(IDFactory.nextId(), controller, spawnTemplate,
// DataManager.GATHERABLE_DATA.getGatherableTemplate(spawnTemplate.getNpcId()), new WorldPosition(spawnTemplate.getWorldId()), true);
// controller.setOwner(this); setKnownlist(new PlayerAwareKnownList(this))

Gatherable::~Gatherable() = default;

controllers::GatherableController& Gatherable::getController() const {
	return static_cast<controllers::GatherableController&>(VisibleObject::getController());
}

const templates::gather::GatherableTemplate* Gatherable::getObjectTemplate() const {
	return static_cast<const templates::gather::GatherableTemplate*>(VisibleObject::getObjectTemplate());
}

} // namespace aion::gameserver::model::gameobjects
