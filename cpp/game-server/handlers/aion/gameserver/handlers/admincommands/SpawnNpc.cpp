#include "aion/gameserver/handlers/admincommands/SpawnNpc.h"

#include "aion/commons/utils/Numbers.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/GatherableData.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/dataholders/SpawnsData.h"
#include "aion/gameserver/model/house/HouseRegistry.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/actions/ItemActions.h"
#include "aion/gameserver/model/templates/item/actions/SummonHouseObjectAction.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/spawnengine/SpawnEngine.h"
#include "aion/gameserver/utils/ChatUtil.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(SpawnNpc);

SpawnNpc::SpawnNpc()
	: AdminCommand("spawn", "Spawns NPCs and gatherables.",
		  "<ID> - Spawns a temporary object with the specified template ID.\n"
		  "<ID> <static ID> [respawn time] - Spawns an object with the specified ID and static ID (default: temporary spawn, optional: respawn "
		  "time in seconds).\n"
		  "<item link|ID> - Spawns the house object from given item link or ID.\n") {
}

// Java SpawnNpc.java:36-66
void SpawnNpc::execute(Player& admin, std::span<const std::string> params) {
	if (params.size() < 1) {
		sendInfo(admin);
		return;
	}

	int32_t itemId = ChatUtil::getItemId(params[0]);
	if (itemId > 0) {
		spawnHouseObject(admin, itemId);
		return;
	}

	int32_t npcId = commons::utils::parseInt(params[0]);
	int32_t staticId = params.size() < 2 ? 0 : commons::utils::parseInt(params[1]);
	int32_t respawnTime = params.size() < 3 ? 0 : commons::utils::parseInt(params[2]);

	if (DataManager::NPC_DATA->getNpcTemplate(npcId) == nullptr && DataManager::GATHERABLE_DATA->getGatherableTemplate(npcId) == nullptr) {
		sendInfo(admin, "Invalid NPC ID.");
		return;
	}
	if (staticId < 0) {
		sendInfo(admin, "Invalid static ID.");
		return;
	}
	if (respawnTime < 0) {
		sendInfo(admin, "Invalid respawn time.");
		return;
	}

	runtime::Ref<SpawnTemplate> st = SpawnEngine::newSpawn(admin.getWorldId(), npcId, admin.getX(), admin.getY(), admin.getZ(), admin.getHeading(), respawnTime);
	st->setStaticId(staticId);
	runtime::Ptr<VisibleObject> visibleObject = SpawnEngine::spawnObject(*st, admin.getInstanceId());
	if (respawnTime > 0 && !DataManager::SPAWNS_DATA->saveSpawn(*visibleObject, false))
		sendInfo(admin, "Could not save spawn. Npc will vanish after server restart.");
}

// Java SpawnNpc.java:68-80
void SpawnNpc::spawnHouseObject(Player& admin, int32_t itemId) {
	const ItemTemplate* itemTemplate = DataManager::ITEM_DATA->getItemTemplate(itemId);
	const ItemActions* actions = itemTemplate == nullptr ? nullptr : itemTemplate->getActions();
	const SummonHouseObjectAction* action = actions == nullptr ? nullptr : actions->getHouseObjectAction();
	if (action == nullptr) {
		sendInfo(admin, "Item is not a spawnable house item.");
		return;
	}
	runtime::Ref<DummyHouseObject> houseObject = VisibleObject::create<DummyHouseObject>(action->getTemplateId());
	houseObject->setPosition(
		World::getInstance().createPosition(admin.getWorldId(), admin.getX(), admin.getY(), admin.getZ(), admin.getHeading(), admin.getInstanceId()));
	SpawnEngine::bringIntoWorld(*houseObject);
}

// ---- DummyHouseObject (SpawnNpc.java:82-111) ---------------------------------------------------------------------------------------------

SpawnNpc::DummyHouseObject::DummyHouseObject(CreateKey key, int32_t templateId)
	: HouseObject(key, runtime::Ptr<model::house::HouseRegistry>(nullptr), IDFactory::getInstance().nextId(), templateId, true) {
}

float SpawnNpc::DummyHouseObject::getX() {
	return getPosition()->getX();
}

float SpawnNpc::DummyHouseObject::getY() {
	return getPosition()->getY();
}

float SpawnNpc::DummyHouseObject::getZ() {
	return getPosition()->getZ();
}

int8_t SpawnNpc::DummyHouseObject::getHeading() {
	return getPosition()->getHeading();
}

} // namespace aion::gameserver::handlers::admincommands
