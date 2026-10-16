#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"

#include "aion/gameserver/runtime/fields/Field.h"

namespace aion::gameserver::handlers::ai::walkers {

/**
 * The AI of Naia ("naia"): on her walk she shouts once when she comes near the cannon (203145) and once near Qydro (203125), again after
 * leaving their range.
 * <p>
 * Java: data/handlers/ai/walkers/NaiaAI.java, @AIName("naia") (the marker is in the .cpp).
 *
 * @author Rolandas
 */
class NaiaAI : public GeneralNpcAI {
private:
	runtime::Field<bool> saidCannon{false};
	runtime::Field<bool> saidQydro{false};

public:
	explicit NaiaAI(Npc& owner) : GeneralNpcAI(owner) {}

protected:
	void handleMoveArrived() override;
};

} // namespace aion::gameserver::handlers::ai::walkers
