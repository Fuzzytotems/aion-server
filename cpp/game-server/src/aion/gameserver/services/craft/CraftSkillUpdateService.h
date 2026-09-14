#pragma once

#include <cstdint>
#include <optional>

#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/craft/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/services/craft/fwd.h"

namespace aion::gameserver::services::craft {

/**
 * C++: an Immortal singleton (hub-headers.md §11.2) with a private constructor and destructor; getInstance() is Java's SingletonHolder.
 *
 * @author MrPoke, sphinx, Imaginary, Pad
 */
class CraftSkillUpdateService : public runtime::Immortal {
private:
	static inline runtime::HashMap<int32_t, model::craft::Profession> professionByNpc{AION_LOCK_CLASS(CraftSkillUpdateService::professionByNpc)}; // Java: = new HashMap<>()
public:
	static CraftSkillUpdateService& getInstance(); // Java singleton
private:
	CraftSkillUpdateService();
	~CraftSkillUpdateService();
public:
	/** @return the profession the npc teaches, std::nullopt if none (Java null) */
	std::optional<model::craft::Profession> getProfessionByNpc(model::gameobjects::Npc& npc);
	void learnSkill(model::gameobjects::player::Player& player, model::gameobjects::Npc& npc);
	int32_t getTotalExpertCraftingSkills(model::gameobjects::player::Player& player);
	int32_t getTotalMasterCraftingSkills(model::gameobjects::player::Player& player);
	bool canLearnMoreExpertCraftingSkill(model::gameobjects::player::Player& player);
	bool canLearnMoreMasterCraftingSkill(model::gameobjects::player::Player& player);
};

} // namespace aion::gameserver::services::craft
