#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"
#include "aion/gameserver/runtime/fields/Field.h"

namespace aion::gameserver::handlers::ai::instance::rentusBase {

/**
 * The AI of the spilled oil in Rentus Base ("spilled_oil"): every 4 s after its spawn it burns with 19658, the seventh time it goes.
 * <p>
 * Java: data/handlers/ai/instance/rentusBase/SpilledOilAI.java, @AIName("spilled_oil") (the marker is in the .cpp).
 *
 * @author xTz
 */
class SpilledOilAI : public GeneralNpcAI {
private:
	runtime::Field<int32_t> count{};

public:
	explicit SpilledOilAI(Npc& owner) : GeneralNpcAI(owner) {}

protected:
	void handleSpawned() override;

private:
	void startEventTask();

	void delete_();
};

} // namespace aion::gameserver::handlers::ai::instance::rentusBase
