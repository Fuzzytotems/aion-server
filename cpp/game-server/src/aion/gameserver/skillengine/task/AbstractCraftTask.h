#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/skillengine/task/AbstractCraftTask_CraftType.h"
#include "aion/gameserver/skillengine/task/AbstractInteractionTask.h"
#include "aion/gameserver/skillengine/task/fwd.h"

namespace aion::gameserver::skillengine::task {

/**
 * The craft-bar half of a timed interaction: every tick either finishes the bar or analyses one more step of it.
 * <p>
 * S0c declaration header (docs/design/hub-headers.md §3.5). Abstract RefCounted (fieldmap K4, same class tree as AbstractInteractionTask);
 * subclasses provide create. The nested Java enum CraftType is the generated `AbstractCraftTask_CraftType`, its `getProgressId()` the free
 * function of AbstractCraftTask_CraftTypeInfo.h.
 *
 * @author ATracer, synchro2
 */
class AbstractCraftTask : public AbstractInteractionTask {
public:
	using CraftType = AbstractCraftTask_CraftType;

protected:
	static constexpr int32_t fullBarValue = 1000;
	runtime::Field<int32_t> currentSuccessValue{};
	runtime::Field<int32_t> currentFailureValue{};
	runtime::Field<int32_t> skillLvlDiff{};
	runtime::Field<CraftType> craftType{CraftType::NORMAL};

	/**
	 * @param responder null: the requester (Java null check of AbstractInteractionTask)
	 */
	AbstractCraftTask(gameserver::model::gameobjects::player::Player& requester,
		runtime::Ptr<gameserver::model::gameobjects::VisibleObject> responder, int32_t skillLvlDiff);
	~AbstractCraftTask() override;

	bool onInteraction() override;

	/**
	 * Perform interaction calculation
	 */
	virtual void analyzeInteraction() = 0;

	virtual void sendInteractionUpdate() = 0;

	virtual bool onSuccessFinish() = 0;

	virtual void onFailureFinish() = 0;
};

} // namespace aion::gameserver::skillengine::task
