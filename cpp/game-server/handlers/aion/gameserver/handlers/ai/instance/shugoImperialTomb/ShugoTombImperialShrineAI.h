#pragma once

#include <cstdint>

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"
#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/sync/Monitor.h"

namespace aion::gameserver::handlers::ai::instance::shugoImperialTomb {

/**
 * The AI of the Shugo Imperial Tomb's Imperial Shrine ("shugo_tomb_imperial_shrine"): choices 1-3 take the Emperor's Golden, Empress' Silver or
 * Crown Prince's Brass Tag (a message without one) and send the player to that tag's treasure room, the first, second and then third room of it
 * for each use; other choices go to the quest engine.
 * <p>
 * Java: data/handlers/ai/instance/shugoImperialTomb/ShugoTombImperialShrineAI.java, @AIName("shugo_tomb_imperial_shrine") (the marker is in the
 * .cpp). Java's synchronized getEntryCountByKeyId is the monitor member.
 *
 * @author Ritsu
 */
class ShugoTombImperialShrineAI : public GeneralNpcAI {
private:
	static constexpr int32_t GOLD_KEY = 182006989;   // Emperor's Golden Tag
	static constexpr int32_t SILVER_KEY = 182006990; // Empress' Silver Tag
	static constexpr int32_t BRONZE_KEY = 182006991; // Crown Prince's Brass Tag
	/** Java: private final Map<Integer, Integer> entriesByKeyId = new HashMap<>() */
	runtime::HashMap<int32_t, int32_t> entriesByKeyId{AION_LOCK_CLASS(ShugoTombImperialShrineAI::entriesByKeyId)};
	runtime::Monitor monitor{AION_LOCK_CLASS(ShugoTombImperialShrineAI::monitor)}; // fieldmap: Java's synchronized getEntryCountByKeyId

public:
	explicit ShugoTombImperialShrineAI(Npc& owner);

protected:
	void handleDialogStart(Player& player) override;

public:
	bool onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) override;

private:
	void enterTreasureRoomIfPossible(Player& player, int32_t dialogActionId);

	int32_t getEntryCountByKeyId(int32_t keyId);
};

} // namespace aion::gameserver::handlers::ai::instance::shugoImperialTomb
