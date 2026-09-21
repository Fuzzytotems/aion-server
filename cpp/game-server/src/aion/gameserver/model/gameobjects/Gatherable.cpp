#include "aion/gameserver/model/gameobjects/Gatherable.h"

#include <memory>
#include <utility>

#include "aion/gameserver/controllers/GatherableController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/GatherableData.h"
#include "aion/gameserver/model/templates/gather/GatherableTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/knownlist/PlayerAwareKnownList.h"

namespace aion::gameserver::model::gameobjects {

Gatherable::Gatherable(CreateKey key, templates::spawns::SpawnTemplate& spawnTemplate,
	std::unique_ptr<controllers::GatherableController> controller)
	: VisibleObject(key, utils::idfactory::IDFactory::getInstance().nextId(), std::move(controller), spawnTemplate,
		  dataholders::DataManager::GATHERABLE_DATA->getGatherableTemplate(spawnTemplate.getNpcId()),
		  world::WorldPosition::create(spawnTemplate.getWorldId()), true) {
	// Java: controller.setOwner(this) (binds the late-bound part before publication; no virtual call on the owner)
	getController().setOwner(*this);
	setKnownlist(std::make_unique<world::knownlist::PlayerAwareKnownList>(*this));
}

Gatherable::~Gatherable() = default;

controllers::GatherableController& Gatherable::getController() const {
	return static_cast<controllers::GatherableController&>(VisibleObject::getController());
}

const templates::gather::GatherableTemplate* Gatherable::getObjectTemplate() const {
	return static_cast<const templates::gather::GatherableTemplate*>(VisibleObject::getObjectTemplate());
}

} // namespace aion::gameserver::model::gameobjects
