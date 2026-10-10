#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/ActionItemNpcAI.h"

#include "aion/gameserver/runtime/fields/Field.h"

namespace aion::gameserver::handlers::ai {

/**
 * The AI of a conquest offering's portal ("conquest_offering_portal": 833018 in Inggison, 833021 in Gelkmaros): it picks a spot of the map's
 * teleport target npcs (856412 / 856433) at least 50 m from its creator's spawn, teleports whoever uses it there and leaves after 65 s.
 * <p>
 * Java: data/handlers/ai/ConquestOfferingPortalAI.java, @AIName("conquest_offering_portal").
 */
class ConquestOfferingPortalAI : public ActionItemNpcAI {
public:
	explicit ConquestOfferingPortalAI(Npc& owner) : ActionItemNpcAI(owner) {}

	void handleSpawned() override;

protected:
	void handleUseItemFinish(Player& player) override;

private:
	runtime::Ref<SpawnTemplate> findTargetLocation();
	runtime::Ptr<Npc> findCreatorNpc();

	/** Java: private SpawnTemplate targetLocation */
	runtime::Field<runtime::Ref<SpawnTemplate>> targetLocation{};
};

} // namespace aion::gameserver::handlers::ai
