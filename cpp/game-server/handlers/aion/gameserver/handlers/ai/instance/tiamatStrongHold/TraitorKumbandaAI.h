#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"
#include "aion/gameserver/runtime/fields/Field.h"

namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold {

/**
 * The AI of Kumbanda the Traitor in Tiamat Stronghold ("traitorkumbanda"): an attack one time in twenty calls, unless one stands, 20726 and
 * seven time accelerators (283086, one on him and six 10-20 m around) and, at 50 % HP or less, unless one stands, his ghost (283085); at 5 %
 * HP 20942 once; his death takes the accelerators and 283088.
 * <p>
 * Java: data/handlers/ai/instance/tiamatStrongHold/TraitorKumbandaAI.java, @AIName("traitorkumbanda") (the marker is in the .cpp).
 *
 * @author Cheatkiller
 */
class TraitorKumbandaAI : public AggressiveNpcAI {
private:
	runtime::Field<bool> isFinalBuff{};

public:
	explicit TraitorKumbandaAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleAttack(runtime::Ptr<Creature> creature) override;

private:
	void spawnTimeAccelerator();

	void spawnKumbandaGhost();

	void deleteNpcs(const std::vector<runtime::Ptr<Npc>>& npcs);

protected:
	void handleDied() override;

	void handleBackHome() override;

private:
	void rndSpawn(int32_t npcId, int32_t count);
};

} // namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold
