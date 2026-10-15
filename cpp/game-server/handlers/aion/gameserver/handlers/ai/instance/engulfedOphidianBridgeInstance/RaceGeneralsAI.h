#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"

#include <vector>

namespace aion::gameserver::handlers::ai::instance::engulfedOphidianBridgeInstance {

/**
 * The AI of the race generals in Engulfed Ophidan Bridge ("engulfedophidiangenerals"): the dialog sends the general and two guards to one
 * of four bases and deletes the other generals of the race.
 * <p>
 * Java: data/handlers/ai/instance/engulfedOphidianBridgeInstance/RaceGeneralsAI.java, @AIName("engulfedophidiangenerals") (the marker is in
 * the .cpp).
 *
 * @author Cheatkiller
 */
class RaceGeneralsAI : public NpcAI {
public:
	explicit RaceGeneralsAI(Npc& owner) : NpcAI(owner) {}

protected:
	void handleDialogStart(Player& player) override;

public:
	bool onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) override;

private:
	void deleteNpcs(const std::vector<runtime::Ptr<Npc>>& npcs);
};

} // namespace aion::gameserver::handlers::ai::instance::engulfedOphidianBridgeInstance
