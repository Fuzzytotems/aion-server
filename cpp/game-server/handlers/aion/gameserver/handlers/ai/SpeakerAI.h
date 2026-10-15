#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"

namespace aion::gameserver::handlers::ai {

/**
 * The AI of a speaker ("speaker": Inggison's and Gelkmaros' heralds): its IDLE pattern shouts only while the siege state they announce
 * holds.
 * <p>
 * Java: data/handlers/ai/SpeakerAI.java, @AIName("speaker"). Java's `pattern == null` is an empty pattern (NpcShoutsService only calls
 * onPatternShout with a pattern).
 */
class SpeakerAI : public GeneralNpcAI {
public:
	explicit SpeakerAI(Npc& owner) : GeneralNpcAI(owner) {}

	bool onPatternShout(ShoutEventType event, std::string_view pattern, int32_t skillNumber) override;

private:
	/** Java: srv.isSiegeInProgress(id) && srv.getSiegeLocation(id).getRace() == race */
	static bool inProgressFor(SiegeService& srv, int32_t id, SiegeRace race);

	/** Java: srv.getSiegeLocation(id) != null && srv.getSiegeLocation(id).getRace() == race */
	static bool heldBy(SiegeService& srv, int32_t id, SiegeRace race);
};

} // namespace aion::gameserver::handlers::ai
