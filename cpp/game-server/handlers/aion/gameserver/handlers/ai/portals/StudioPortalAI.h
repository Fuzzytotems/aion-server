#pragma once

#include <optional>

#include "aion/gameserver/handlers/ai/ActionItemNpcAI.h"
#include "aion/gameserver/handlers/ai/AiPrelude.h"

namespace aion::gameserver::handlers::ai::portals {

/**
 * The AI of a studio portal ("studioportal"): after ActionItemNpcAI's use bar it takes the player into his own studio (a personal instance,
 * created on demand) or, from inside a studio, out to the studio's exit point.
 * <p>
 * Java: data/handlers/ai/portals/StudioPortalAI.java, @AIName("studioportal") (the marker is in the .cpp). M5h HS-2, an A1 lease.
 *
 * @author Rolandas
 */
class StudioPortalAI : public ActionItemNpcAI {
public:
	explicit StudioPortalAI(Npc& owner) : ActionItemNpcAI(owner) {}

	bool onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) override;

protected:
	void handleUseItemFinish(Player& player) override;

private:
	/** Java auto-unboxing of a null Integer/Float of the address (the exit attributes are optional in the schema): NullPointerException */
	template <class T>
	static T unbox(const std::optional<T>& value);
};

} // namespace aion::gameserver::handlers::ai::portals
