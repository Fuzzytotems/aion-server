#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"

namespace aion::gameserver::handlers::ai::siege {

/**
 * A fortress's mercenary manager ("mercenary"): the owning legion's members, while the fortress is vulnerable, pick a mercenary zone in its
 * dialog and pay its cost in 186000236 to spawn the zone's mercenaries; everyone else gets page 1011.
 * <p>
 * Java: data/handlers/ai/siege/MercenaryAI.java, @AIName("mercenary") (the marker is in the .cpp).
 *
 * @author ViAl, Whoop
 */
class MercenaryAI : public GeneralNpcAI {
public:
	explicit MercenaryAI(Npc& owner) : GeneralNpcAI(owner) {}

protected:
	void handleDialogStart(Player& player) override;

public:
	bool onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) override;

private:
	void checkMercenaryZone(Player& player, int32_t siegeId, int32_t zoneId);
};

} // namespace aion::gameserver::handlers::ai::siege
