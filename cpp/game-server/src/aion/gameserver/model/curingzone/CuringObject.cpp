#include "aion/gameserver/model/curingzone/CuringObject.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/controllers/VisibleObjectController.h"
#include "aion/gameserver/model/gameobjects/AionObject.h"
#include "aion/gameserver/model/templates/VisibleObjectTemplate.h"
#include "aion/gameserver/model/templates/curingzones/CuringTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::model::curingzone {
// anonymous VisibleObjectController at CuringObject.java:19 (model.curingzone.CuringObject$1); argument 2 of super(); storage: stored in
// VisibleObject

CuringObject::CuringObject(CreateKey key, const templates::curingzones::CuringTemplate* value, int32_t instanceId)
	: gameobjects::VisibleObject(key,
	int32_t{}, nullptr, runtime::Ptr<templates::spawns::SpawnTemplate>{}, static_cast<const templates::VisibleObjectTemplate*>(nullptr),
	runtime::Ptr<world::WorldPosition>{}, bool{}), template_(value), range() {
	// Java: super(IDFactory.getInstance().nextId(), new VisibleObjectController<CuringObject>() { }, null, null,
	// World.getInstance().createPosition(template.getMapId(), template.getX(), template.getY(), template.getZ(), (byte) 0, instanceId), true);
	// this.range = template.getRange(); setKnownlist(new NpcKnownList(this)); super(...) arguments
	AION_UNPORTED();
}

std::string CuringObject::getName() {
	AION_UNPORTED();
}

void CuringObject::spawn() {
	AION_UNPORTED();
}

CuringObject::~CuringObject() = default;

} // namespace aion::gameserver::model::curingzone
