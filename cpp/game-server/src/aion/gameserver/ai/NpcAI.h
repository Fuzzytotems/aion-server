#pragma once

#include <cstdint>
#include <optional>
#include <set>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/ai/AITemplate.h"
#include "aion/gameserver/ai/poll/fwd.h"
#include "aion/gameserver/controllers/attack/fwd.h"
#include "aion/gameserver/controllers/effect/fwd.h"
#include "aion/gameserver/controllers/movement/fwd.h"
#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/skill/fwd.h"
#include "aion/gameserver/model/stats/container/fwd.h"
#include "aion/gameserver/model/templates/npc/fwd.h"
#include "aion/gameserver/model/templates/spawns/fwd.h"
#include "aion/gameserver/world/knownlist/fwd.h"

namespace aion::gameserver::ai {

/**
 * Base of all NPC AIs: owner accessors and the default NPC event handling (spawn, shouts, walking, AP rewards).
 * <p>
 * Hub header (docs/design/hub-headers.md). A non-template class deriving AITemplate&lt;Npc&gt; (OwnerType = Npc): getOwner() returns Npc&.
 * The owner accessors return what the Npc/Creature accessors they delegate to return (getAggroList, getKnownList: references; getSkillList,
 * getLifeStats, getEffectController, getMoveController: Ptr, as Npc and Creature declare them; getTribe: std::optional like Npc::getTribe), the
 * templates as `const X*`. The constructor is protected (Java abstract class): the AI registry creates leaf AIs with `std::make_unique<C>(npc)`,
 * and `AION_AI(NpcAI, ...)` does not satisfy AIHandlerClass.
 *
 * @author ATracer
 */
class NpcAI : public AITemplate<model::gameobjects::Npc> {
private:
	/** Java: EnumSet.of(Race.ASMODIANS, Race.DARK, ...) */
	static const std::set<model::Race> apRewardingRaces;

protected:
	explicit NpcAI(model::gameobjects::Npc& owner);

	const model::templates::npc::NpcTemplate* getObjectTemplate();

	runtime::Ptr<model::templates::spawns::SpawnTemplate> getSpawnTemplate();

	runtime::Ptr<model::stats::container::NpcLifeStats> getLifeStats();

	model::Race getRace();

	/** @return Npc::getTribe(), std::nullopt for NPC templates without a tribe (Java null) */
	std::optional<model::TribeClass> getTribe();

	runtime::Ptr<controllers::effect::EffectController> getEffectController();

	world::knownlist::KnownList& getKnownList();

	controllers::attack::AggroList& getAggroList();

	runtime::Ptr<model::skill::NpcSkillList> getSkillList();

	runtime::Ptr<model::gameobjects::VisibleObject> getCreator();

	/**
	 * DEPRECATED as movements will be processed as commands only from ai
	 */
	runtime::Ptr<controllers::movement::NpcMoveController> getMoveController();

	int32_t getNpcId();

	int32_t getCreatorId();

	bool isInRange(model::gameobjects::VisibleObject& object, int32_t range);

	void handleActivate() override;

	void handleDeactivate() override;

	void handleBeforeSpawned() override;

	void handleSpawned() override;

	void handleDespawned() override;

	void handleDied() override;

	void handleMoveArrived() override;

	void handleTargetChanged(model::gameobjects::Creature& creature) override;

public:
	bool ask(poll::AIQuestion question) override;

	bool isDestinationReached() override;

protected:
	void handleMoveValidate() override;

	void handleCreatureMoved(model::gameobjects::Creature& creature) override;

public:
	virtual bool isMoveSupported();

	/**
	 * NCsoft uses different non-visible npcs as a sensor to trigger different events
	 */
	virtual void handleCreatureDetected(model::gameobjects::Creature& creature) {}
};

} // namespace aion::gameserver::ai
