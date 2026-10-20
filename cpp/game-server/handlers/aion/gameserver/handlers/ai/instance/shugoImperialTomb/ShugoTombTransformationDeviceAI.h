#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/ActionItemNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::shugoImperialTomb {

/**
 * The AI of the Shugo Imperial Tomb's transformation devices ("shugo_tomb_transformation_device"): used, 831097 gives the third stage's
 * transformation 1 s later, 831096 the second's (by the player's race), then the use finishes as any item npc's.
 * <p>
 * Java: data/handlers/ai/instance/shugoImperialTomb/ShugoTombTransformationDeviceAI.java, @AIName("shugo_tomb_transformation_device") (the
 * marker is in the .cpp).
 *
 * @author Estrayl
 */
class ShugoTombTransformationDeviceAI : public ActionItemNpcAI {
public:
	explicit ShugoTombTransformationDeviceAI(Npc& owner) : ActionItemNpcAI(owner) {}

protected:
	void handleUseItemFinish(Player& player) override;

private:
	void applyTransformationEffect(Player& player);
};

} // namespace aion::gameserver::handlers::ai::instance::shugoImperialTomb
