#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"

#include <optional>
#include <string_view>

namespace aion::gameserver::handlers::ai::events::iceFestival {

/**
 * The AI of the ice chunks of the ice festival ("icefestival_ice_chunk"): the dialog uses the player's ice carving tool; its skill (11014)
 * carves the chunk: 69 % the next stage (and a box), the fifth a completed sculpture for two hours, else it shatters and comes back after
 * two minutes - while the event runs; it never respawns.
 * <p>
 * Java: data/handlers/ai/events/iceFestival/IceChunkAI.java, @AIName("icefestival_ice_chunk") (the marker is in the .cpp).
 *
 * @author (none)
 */
class IceChunkAI : public NpcAI {
public:
	explicit IceChunkAI(Npc& owner) : NpcAI(owner) {}

protected:
	void handleDialogStart(Player& player) override;

public:
	void onEffectApplied(Effect& effect) override;

private:
	runtime::Ptr<VisibleObject> spawn(int32_t npcId, std::optional<std::string> aiName);

	using NpcAI::spawn;

public:
	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::events::iceFestival
