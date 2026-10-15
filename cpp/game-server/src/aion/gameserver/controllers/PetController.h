#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/controllers/VisibleObjectController.h"
#include "aion/gameserver/controllers/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"

namespace aion::gameserver::controllers {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). The controller part of Pet (`std::make_unique<PetController>()`, late-bound by
 * setOwner in Pet::postConstruct). Binds VisibleObjectController's type variable to Pet: getOwner() returns `Pet&` (§8.2).
 * <p>
 * PetUpdateTask is the Runnable PetSpawnService schedules at a fixed rate: it keeps startTime between runs, so it is RefCounted and holds its
 * player by Ref (fieldmap.toml [kinds] K4, like CreatureController.DelayedOnAttack).
 *
 * @author ATracer
 */
class PetController : public VisibleObjectController {
public:
	/** Java: public static class PetUpdateTask implements Runnable */
	class PetUpdateTask final : public runtime::RefCounted {
		AION_MAKE_REF_FRIEND
	private:
		const runtime::Ref<model::gameobjects::player::Player> player; // Java: private final Player player
		runtime::Field<int64_t> startTime{0};                         // Java: private long startTime

	protected:
		explicit PetUpdateTask(model::gameobjects::player::Player& player);
		~PetUpdateTask() override;

	public:
		/** Java: new PetUpdateTask(player) */
		static runtime::Ref<PetUpdateTask> create(model::gameobjects::player::Player& player);

		void run();
	};

	PetController();
	~PetController() override;

	/** Narrowing accessor (Java: VisibleObjectController<Pet>.getOwner(), hub-headers.md §8.2). */
	model::gameobjects::Pet& getOwner() const;

	void onDelete() override;
};

} // namespace aion::gameserver::controllers
