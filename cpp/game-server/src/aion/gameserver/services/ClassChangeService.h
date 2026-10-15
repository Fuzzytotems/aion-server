#pragma once

#include <cstdint>
#include <optional>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/services/fwd.h"

namespace aion::gameserver::services {

/**
 * C++: a static-only class (hub-headers.md §11.1).
 *
 * @author ATracer, sweetkr, Neon
 */
class ClassChangeService {
public:
	static void showClassChangeDialog(model::gameobjects::player::Player& player);
	static void changeClassToSelection(model::gameobjects::player::Player& player, int32_t dialogActionId);
	static void completeAscensionQuest(model::gameobjects::player::Player& player);
	static bool setClass(model::gameobjects::player::Player& player, model::PlayerClass newClass);
	/** @param newClass null for an unknown dialog selection (changeClassToSelection); Java tests `newClass == null` */
	static bool setClass(model::gameobjects::player::Player& player, std::optional<model::PlayerClass> newClass, bool validate, bool updateDaevaStatus);
	static int32_t getClassSelectionDialogPageId(model::Race playerRace, model::PlayerClass playerClass);
	/** @return the selected class, null for an unknown dialog action */
	static std::optional<model::PlayerClass> getSelectedPlayerClass(model::Race race, int32_t dialogActionId);
};

} // namespace aion::gameserver::services
