#include "aion/gameserver/model/gameobjects/NpcObject.h"

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/house/House.h"
#include "aion/gameserver/model/house/HouseRegistry.h"
#include "aion/gameserver/model/templates/housing/HousingNpc.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/runtime/sync/Monitor.h"
#include "aion/gameserver/spawnengine/SpawnEngine.h"

namespace aion::gameserver::model::gameobjects {

NpcObject::NpcObject(CreateKey key, house::HouseRegistry& registry, int32_t objId, int32_t templateId)
	: HouseObject(key, registry, objId, templateId) {
}

NpcObject::~NpcObject() = default;

const templates::housing::HousingNpc* NpcObject::getObjectTemplate() const {
	return static_cast<const templates::housing::HousingNpc*>(HouseObject::getObjectTemplate());
}

void NpcObject::onUse(player::Player& player) {
	// TODO: Talk ?
}

void NpcObject::spawn() {
	SYNCHRONIZED(*this) {
		HouseObject::spawn();
		if (!npc.get()) {
			const templates::housing::HousingNpc* template_ = getObjectTemplate();
			runtime::Ref<templates::spawns::SpawnTemplate> spawn = spawnengine::SpawnEngine::newSingleTimeSpawn(getOwnerHouse()->getWorldId(),
				template_->getNpcId(), getX(), getY(), getZ(), getHeading());
			npc.set(runtime::cast<Npc>(spawnengine::SpawnEngine::spawnObject(*spawn, getOwnerHouse()->getInstanceId())));
		}
	}
}

void NpcObject::onDespawn() {
	SYNCHRONIZED(*this) {
		HouseObject::onDespawn();
		if (runtime::Ptr<Npc> spawned = npc.get()) {
			spawned->getController().delete_();
			npc.set(nullptr);
		}
	}
}

bool NpcObject::canExpireNow() {
	SYNCHRONIZED(*this) {
		runtime::Ptr<Npc> spawned = npc.get();
		if (!spawned)
			return true;
		return !spawned->getTarget();
	}
}

int32_t NpcObject::getNpcObjectId() {
	runtime::Ptr<Npc> spawned = npc.get();
	return !spawned ? 0 : spawned->getObjectId();
}

} // namespace aion::gameserver::model::gameobjects
