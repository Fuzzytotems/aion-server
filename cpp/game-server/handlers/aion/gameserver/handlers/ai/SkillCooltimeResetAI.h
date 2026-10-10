#pragma once

#include <cstdint>
#include <set>
#include <string>

#include "aion/gameserver/handlers/ai/AiPrelude.h"

#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"

namespace aion::gameserver::handlers::ai {

struct SkillCooltimeResetAI_AIRequest;

/**
 * The AI of the custom cooldown resetter ("customcdreset"): for 50,000 kinah it heals a player and resets his skill cooldowns of at most
 * 3060 (5 min 6 s) and his buff item and potion cooldowns of at most 300 s. It greets each player who comes within 8 m in its sight (once per
 * 5 minutes); on a PvP map it leaves after 30 s or after one use.
 * <p>
 * Java: data/handlers/ai/SkillCooltimeResetAI.java, @AIName("customcdreset"). Java's anonymous AIRequest is the callback struct
 * SkillCooltimeResetAI_AIRequest of the .cpp (fieldmap ai.SkillCooltimeResetAI$1), which calls the private collectors as a friend. Java's
 * HashSet<Integer> of cooldown ids is a std::set: the ids are only removed and handed to SM_SKILL_COOLDOWN and SM_ITEM_COOLDOWN, which order
 * them themselves. String.format("%,d", PRICE) groups with ',' (the server's default locale is English).
 */
class SkillCooltimeResetAI : public NpcAI {
	friend struct SkillCooltimeResetAI_AIRequest;

public:
	explicit SkillCooltimeResetAI(Npc& owner) : NpcAI(owner) {}

	void handleCreatureMoved(Creature& creature) override;

protected:
	void handleSpawned() override;

	void handleDialogStart(Player& player) override;

private:
	static constexpr int32_t PRICE = 50'000;
	static constexpr int32_t MAX_SKILL_COOLDOWN_TIME = 3060; // = 5min 6sec
	static constexpr int32_t MAX_ITEM_COOLDOWN_SECONDS = 300; // excludes items like Fine Bracing Water, Leader's Recovery Scroll, Recovery Crystal etc.

	void tryNotify(Player& player);

	void sendRequest(Player& player);

	std::set<int32_t> collectResettableItemCooldownIds(Player& player);

	std::set<int32_t> collectBuffItemAndPotionCooldownIds(Player& player);

	std::set<int32_t> collectResettableSkillCooldownIds(Player& player);

	/** Java String.format("%,d", value) in an English locale */
	static std::string formatGrouped(int64_t value);

	/** Java: private final Map<Integer, Long> playersInSight = new ConcurrentHashMap<>() (object id -> first sight millis) */
	runtime::ConcurrentHashMap<int32_t, int64_t> playersInSight{AION_LOCK_CLASS(SkillCooltimeResetAI::playersInSight)};
};

} // namespace aion::gameserver::handlers::ai
