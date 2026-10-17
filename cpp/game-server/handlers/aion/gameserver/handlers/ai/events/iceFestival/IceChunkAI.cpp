#include "aion/gameserver/handlers/ai/events/iceFestival/IceChunkAI.h"

#include <string>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/event/EventTemplate.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/actions/ItemActions.h"
#include "aion/gameserver/model/templates/item/actions/SkillUseAction.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/services/event/EventService.h"
#include "aion/gameserver/services/item/ItemService.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::events::iceFestival {

AION_AI(IceChunkAI, "icefestival_ice_chunk");

// Java IceChunkAI.java:28-35 (Java's cast of the first item action to SkillUseAction: a ClassCastException for another action)
void IceChunkAI::handleDialogStart(Player& player) {
	runtime::Ptr<model::gameobjects::Item> iceCarvingTool = player.getInventory().getFirstItemByItemId(164002294);
	if (iceCarvingTool != nullptr) {
		const auto& actions = iceCarvingTool->getItemTemplate()->getActions()->getItemActions();
		if (actions.empty())
			throw runtime::NoSuchElementException("IceChunkAI: the ice carving tool has no action"); // Java: getFirst() of an empty list
		const auto& skillUseAction = dynamic_cast<const model::templates::item::actions::SkillUseAction&>(*actions.front());
		skillUseAction.act(player, iceCarvingTool, nullptr);
	}
}

// Java IceChunkAI.java:37-66
void IceChunkAI::onEffectApplied(Effect& effect) {
	if (getNpcId() < 833516 || getNpcId() > 833519)
		return;
	runtime::Ptr<Player> player = runtime::as<Player>(effect.getEffector());
	if (effect.getSkillId() == 11014 && player != nullptr && getOwner().getController().delete_()) {
		if (commons::utils::Rnd::chance() < 69) {
			switch (getNpcId()) {
				case 833516:
					ItemService::addItem(*player, 188053955, 1, true); // [Event] Ice Sculptor's Box
					break;
				case 833517:
					ItemService::addItem(*player, 188053956, 1, true); // [Event] Talented Ice Sculptor's Box
					break;
				case 833518:
					ItemService::addItem(*player, 188053957, 1, true); // [Event] Enheartened Ice Sculptor's Box
					break;
				case 833519:
					ItemService::addItem(*player, 188053958, 1, true); // [Event] Soulful Ice Sculptor's Box
					break;
			}
			int32_t nextId = getNpcId() + 1;
			bool isCompletedIceSculpture = nextId == 833520;
			if (isCompletedIceSculpture) {
				PacketSendUtility::sendMonologue(*player, 1501360); // The ice sculpture is a great success!
				runtime::Ptr<Npc> completedIceSculpture = runtime::cast<Npc>(spawn(nextId, std::string("icefestival_completed_ice_sculpture")));
				runtime::Ref<Npc> sculpture(*completedIceSculpture);
				completedIceSculpture->getController().addTask(TaskId::DESPAWN, ThreadPoolManager::getInstance().schedule({this}, [this, sculpture] {
					sculpture->getController().delete_();
					spawn(833516, getSpawnTemplate()->getAiName());
				}, 2 * 60 * 60 * 1000));
			} else {
				spawn(nextId, getSpawnTemplate()->getAiName());
			}
		} else {
			PacketSendUtility::sendMonologue(*player, 1501361); // Oh, the ice sculpture has shattered...
			ThreadPoolManager::getInstance().schedule({this}, [this] { spawn(833516, getSpawnTemplate()->getAiName()); }, 2 * 60 * 1000);
		}
	}
}

// Java IceChunkAI.java:68-72
runtime::Ptr<VisibleObject> IceChunkAI::spawn(int32_t npcId, std::optional<std::string> aiName) {
	if (!services::event::EventService::getInstance().isEventActive(getSpawnTemplate()->getEventTemplate()->getName()))
		return nullptr;
	std::optional<std::string_view> name = aiName.has_value() ? std::optional<std::string_view>(*aiName) : std::nullopt;
	return NpcAI::spawn(npcId, getPosition()->getX(), getPosition()->getY(), getPosition()->getZ(), getPosition()->getHeading(), 0, name);
}

// Java IceChunkAI.java:74-79
bool IceChunkAI::ask(AIQuestion question) {
	if (question == AIQuestion::ALLOW_RESPAWN)
		return false;
	return NpcAI::ask(question);
}

} // namespace aion::gameserver::handlers::ai::events::iceFestival
