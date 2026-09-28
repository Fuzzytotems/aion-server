#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/services/fwd.h"

namespace aion::gameserver::services {

/**
 * C++: a static-only class (hub-headers.md §11.1).
 *
 * @author VladimirZ, Neon
 */
class DialogService {
public:
	static void onCloseDialog(model::gameobjects::player::Player& player, runtime::Ptr<model::gameobjects::VisibleObject> target);
	static void onDialogSelect(int32_t dialogActionId, model::gameobjects::player::Player& player, model::gameobjects::Npc& npc, int32_t questId, int32_t extendedRewardIndex);
private:
	static void handleQuestDialogueOrSendNextPage(int32_t dialogActionId, model::gameobjects::player::Player& player, model::gameobjects::Npc& npc, int32_t questId, int32_t extendedRewardIndex);
	static void sendDialogWindow(int32_t dialogActionId, model::gameobjects::player::Player& player, model::gameobjects::Npc& npc);
public:
	static bool isInteractionAllowed(model::gameobjects::player::Player& player, model::gameobjects::Npc& npc);
private:
	static bool isSummonOwner(model::gameobjects::player::Player& player, model::gameobjects::Npc& npc);
	static bool isSubDialogRestricted(model::gameobjects::player::Player& player, model::gameobjects::Npc& npc);
};

} // namespace aion::gameserver::services
