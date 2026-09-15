#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/controllers/VisibleObjectController.h"
#include "aion/gameserver/controllers/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/templates/gather/fwd.h"
#include "aion/gameserver/skillengine/task/fwd.h"

namespace aion::gameserver::controllers {

/**
 * Starts, completes, rewards and cancels gathering on a gatherable.
 * <p>
 * The controller part of Gatherable (late-bound by setOwner). Binds VisibleObjectController's type variable to Gatherable: getOwner() returns
 * `Gatherable&` (hub-headers.md §8.2). The Java synchronized (this) blocks are SYNCHRONIZED(*this) on the part (the owner's monitor).
 * <p>
 * Partly defined: the constructor, the destructor and the bodies that store, read or abort `gatheringTask` need the complete GatheringTask
 * (P5-02), which has no declaration header yet (skillengine/task/GatheringTask.h). The constructor and destructor stay undefined, so nothing may
 * create a GatherableController yet (the Gatherable constructor of P4-11a is not defined either); the task bodies are unported. The other bodies
 * are ported in GatherableController.cpp.
 *
 * @author ATracer, sphinx, Cura
 */
class GatherableController : public VisibleObjectController {
private:
	runtime::Field<int32_t> gatherCount{};
	runtime::Field<runtime::Ref<skillengine::task::GatheringTask>> gatheringTask{};

public:
	/** Java: implicit default constructor */
	GatherableController();
	~GatherableController() override;

	/** Narrowing accessor (Java: VisibleObjectController<Gatherable>.getOwner(), hub-headers.md §8.2). */
	model::gameobjects::Gatherable& getOwner() const;

	void startGathering(model::gameobjects::player::Player& player);

private:
	/**
	 * Checks whether player have needed skill for gathering and skill level is sufficient
	 */
	bool checkPlayerSkill(model::gameobjects::player::Player& player, const model::templates::gather::GatherableTemplate* template_);

	/** @return the materials to gather from, nullptr (Java null) if the required item is missing */
	const std::vector<model::templates::gather::Material>* getMaterials(model::gameobjects::player::Player& player,
		const model::templates::gather::GatherableTemplate* template_);

public:
	void completeInteraction(); // synchronized (this)

	/** player: nullable (Java null check) */
	void rewardPlayer(runtime::Ptr<model::gameobjects::player::Player> player);

	void onDespawn() override;

	void cancelGathering(); // synchronized (this)

	int32_t getGatheringPlayerId(); // synchronized (this)
};

} // namespace aion::gameserver::controllers
