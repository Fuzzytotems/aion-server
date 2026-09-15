#include "aion/gameserver/model/gameobjects/Gatherable.h"

#include "aion/gameserver/model/templates/gather/GatherableTemplate.h"

namespace aion::gameserver::model::gameobjects {

// Gatherable(CreateKey, SpawnTemplate&, std::unique_ptr<controllers::GatherableController>) and getController() are defined once
// controllers/GatherableController.h exists (P4-11b). Java: super(IDFactory.nextId(), controller, spawnTemplate,
// DataManager.GATHERABLE_DATA.getGatherableTemplate(spawnTemplate.getNpcId()), new WorldPosition(spawnTemplate.getWorldId()), true);
// controller.setOwner(this); setKnownlist(new PlayerAwareKnownList(this))

Gatherable::~Gatherable() = default;

const templates::gather::GatherableTemplate* Gatherable::getObjectTemplate() const {
	return static_cast<const templates::gather::GatherableTemplate*>(VisibleObject::getObjectTemplate());
}

} // namespace aion::gameserver::model::gameobjects
