#include "aion/gameserver/handlers/ai/ConquestOfferingPortalAI.h"

#include <vector>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/SpawnsData.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/animations/TeleportAnimation.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai {

AION_AI(ConquestOfferingPortalAI, "conquest_offering_portal");

// Java ConquestOfferingPortalAI.java:30-35. The despawn task is pinned on this AI (a part of its npc).
void ConquestOfferingPortalAI::handleSpawned() {
	ActionItemNpcAI::handleSpawned();
	targetLocation.set(findTargetLocation());
	getOwner().getController().addTask(model::TaskId::DESPAWN,
		ThreadPoolManager::getInstance().schedule({this}, [this] { getOwner().getController().delete_(); }, 65000));
}

// Java ConquestOfferingPortalAI.java:37-42
void ConquestOfferingPortalAI::handleUseItemFinish(Player& player) {
	runtime::Ref<SpawnTemplate> target = targetLocation.get();
	if (target != nullptr)
		services::teleport::TeleportService::teleportTo(player, target->getWorldId(), target->getX(), target->getY(), target->getZ(),
			target->getHeading(), model::animations::TeleportAnimation::FADE_OUT_BEAM);
}

// Java ConquestOfferingPortalAI.java:44-66
runtime::Ref<SpawnTemplate> ConquestOfferingPortalAI::findTargetLocation() {
	int32_t npcId = getNpcId() == 833018 ? 856412 : 856433;
	std::optional<runtime::Ref<SpawnGroup>> spawnGroup = commons::utils::Rnd::get(DataManager::SPAWNS_DATA->getSpawnsForNpc(getOwner().getWorldId(), npcId));
	if (spawnGroup && *spawnGroup != nullptr) {
		runtime::Ref<SpawnTemplate> location; // Java: the local targetLocation (renamed: it would hide the field)
		runtime::Ptr<Npc> creator = findCreatorNpc();
		if (creator != nullptr) {
			runtime::Ptr<SpawnTemplate> creatorTemplate = creator->getSpawn();
			// exclude all teleport templates within a 50m range around the creator spawn template
			// to prevent teleportation to the killed conquest npc (creator of this npc)
			std::vector<runtime::Ptr<SpawnTemplate>> spawnTemplates;
			for (const runtime::Ptr<SpawnTemplate>& teleportTemplate : (*spawnGroup)->getSpawnTemplates().snapshot())
				if (!PositionUtil::isInRange(teleportTemplate->getX(), teleportTemplate->getY(), teleportTemplate->getZ(), creatorTemplate->getX(),
						creatorTemplate->getY(), creatorTemplate->getZ(), 50))
					spawnTemplates.push_back(teleportTemplate);
			const runtime::Ptr<SpawnTemplate>* picked = commons::utils::Rnd::get(spawnTemplates);
			if (picked != nullptr)
				location = runtime::Ref<SpawnTemplate>(*picked);
		}
		if (location != nullptr)
			return location;
		std::vector<runtime::Ptr<SpawnTemplate>> all = (*spawnGroup)->getSpawnTemplates().snapshot();
		const runtime::Ptr<SpawnTemplate>* any = commons::utils::Rnd::get(all);
		return any == nullptr ? runtime::Ref<SpawnTemplate>() : runtime::Ref<SpawnTemplate>(*any);
	}
	return nullptr;
}

// Java ConquestOfferingPortalAI.java:68-72
runtime::Ptr<Npc> ConquestOfferingPortalAI::findCreatorNpc() {
	if (getCreatorId() != 0)
		return runtime::as<Npc>(getPosition()->getWorldMapInstance()->getObject(getCreatorId())); // Java: instanceof Npc npc ? npc : (falls to null)
	return nullptr;
}

} // namespace aion::gameserver::handlers::ai
