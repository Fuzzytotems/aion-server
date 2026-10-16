#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/classNpc/DrakanPriestAI.h"

namespace aion::gameserver::handlers::ai::classNpc {

/**
 * The AI of the drakan medics ("drakanmedic"): a drakan priest that may also (3 %) summon one healing servant (281621, or 281839 for a
 * rating other than NORMAL).
 * <p>
 * Java: data/handlers/ai/classNpc/DrakanMedicAI.java, @AIName("drakanmedic") (the marker is in the .cpp).
 *
 * @author Cheatkiller
 */
class DrakanMedicAI : public DrakanPriestAI {
public:
	explicit DrakanMedicAI(Npc& owner) : DrakanPriestAI(owner) {}

protected:
	void handleAttack(runtime::Ptr<Creature> creature) override;
};

} // namespace aion::gameserver::handlers::ai::classNpc
