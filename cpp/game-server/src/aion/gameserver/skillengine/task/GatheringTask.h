#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/controllers/observer/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/templates/gather/fwd.h"
#include "aion/gameserver/skillengine/task/AbstractCraftTask.h"
#include "aion/gameserver/skillengine/task/fwd.h"

namespace aion::gameserver::skillengine::task {

/**
 * One gathering interaction of a player with a Gatherable: the craft bar, its packets and the reward.
 * <p>
 * S0c declaration header (docs/design/hub-headers.md §3.5). RefCounted (fieldmap K4: held by `GatherableController::gatheringTask` and
 * `Player::interactionTask`), created with create(). Its existence is what makes a GatherableController constructible, and with it every
 * Gatherable of the world (header request world-engines-1 / hr-2 of the world-visibility lane, m5a-plan.md W-04).
 * <p>
 * C++ deviations (docs/deviations/P5-02.md):
 * - `gathererObserver` is a `Field` instead of Java's final field (fieldmap.toml, cycles.toml cpp-breaker): onInteractionFinish resets it
 *   after removing the observer, otherwise the task and its observer keep each other alive.
 * - Java's constructor calls createGathererObserver(), which captures `this`; a Ref to a RefCounted cannot be taken inside its own
 *   constructor, so create() attaches the observer right after construction (two-phase construction, handlers-and-porting-plan.md §1.7).
 *
 * @author ATracer, Yeats
 */
class GatheringTask final : public AbstractCraftTask {
	AION_MAKE_REF_FRIEND
private:
	const gameserver::model::templates::gather::GatherableTemplate* template_;
	/** C++ only mutable (fieldmap.toml): reset by onInteractionFinish once the observer is detached */
	runtime::Field<runtime::Ref<controllers::observer::ActionObserver>> gathererObserver{};
	const gameserver::model::templates::gather::Material* material;
	runtime::Field<int32_t> showBarDelay{};
	runtime::Field<int32_t> executionSpeed{};

protected:
	GatheringTask(gameserver::model::gameobjects::player::Player& requester, gameserver::model::gameobjects::Gatherable& gatherable,
		const gameserver::model::templates::gather::Material* material, int32_t skillLvlDiff);
	~GatheringTask() override;

public:
	/** Java: new GatheringTask(requester, gatherable, material, skillLvlDiff) */
	static runtime::Ref<GatheringTask> create(gameserver::model::gameobjects::player::Player& requester,
		gameserver::model::gameobjects::Gatherable& gatherable, const gameserver::model::templates::gather::Material* material,
		int32_t skillLvlDiff);

protected:
	void onInteractionAbort() override;

	void onInteractionFinish() override;

	void onInteractionStart() override;

	void sendInteractionUpdate() override;

	void onFailureFinish() override;

	bool onSuccessFinish() override;

	void analyzeInteraction() final;

public:
	int32_t getGathererId();

private:
	runtime::Ref<controllers::observer::ActionObserver> createGathererObserver();
};

} // namespace aion::gameserver::skillengine::task
