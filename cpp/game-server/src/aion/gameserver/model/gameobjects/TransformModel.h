#pragma once

#include <cstdint>
#include <optional>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Parts.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::model::gameobjects {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). A part of Creature (`PartSlot<TransformModel>`, created lazily by
 * Creature::getTransformModel), bound to its creature in the constructor (ported: it only stores the original transform type). The tribe is
 * null until setTribe and PolymorphEffect.java:33 stores null: `Field<std::optional<TribeClass>>` (hub-headers.md §6).
 *
 * @author Rolandas
 */
class TransformModel : public runtime::OwnedPart {
private:
	runtime::OwnerRef<Creature> owner;
	runtime::Field<int32_t> modelId{};
	runtime::Field<int32_t> eventModelId{};
	const skillengine::model::TransformType originalType;
	runtime::Field<skillengine::model::TransformType> transformType;
	runtime::Field<int32_t> panelId{};
	runtime::Field<std::optional<TribeClass>> transformTribe{}; // fieldmap.toml: Java stores null (PolymorphEffect.java:33), hub-headers.md §6

protected:
	// restrictions
	runtime::Field<bool> cantUseSkills_{};
	runtime::Field<bool> cantMove_{};
	runtime::Field<bool> cantRecall_{};
	runtime::Field<bool> cantJump_{};
	runtime::Field<bool> cantAttack_{};
	runtime::Field<bool> cantUseItems_{};
	runtime::Field<bool> cantFly_{};

public:
	explicit TransformModel(Creature& creature);

	~TransformModel() override;

	void apply(int32_t modelId);

	void apply(int32_t modelId, skillengine::model::TransformType type, int32_t panelId, bool cantUseSkills, bool cantMove, bool cantRecall,
		bool cantJump, bool cantAttack, bool cantUseItems, bool cantFly);

	void updateVisually();

private:
	void updateTribeVisually();

public:
	int32_t getModelId();

	bool isUnrestricted();

	void setEventModelId(int32_t value) { eventModelId.set(value); }

	int32_t getEventModelId() const { return eventModelId.get(); }

	skillengine::model::TransformType getType() const { return transformType.get(); }

	int32_t getPanelId() const { return panelId.get(); }

	bool isActive();

	std::optional<TribeClass> getTribe() const { return transformTribe.get(); }

	void setTribe(std::optional<TribeClass> transformTribe);

	bool cantUseSkills() const { return cantUseSkills_.get(); }

	bool cantMove() const { return cantMove_.get(); }

	bool cantRecall() const { return cantRecall_.get(); }

	bool cantJump() const { return cantJump_.get(); }

	bool cantAttack() const { return cantAttack_.get(); }

	bool cantUseItems() const { return cantUseItems_.get(); }

	bool cantFly() const { return cantFly_.get(); }
};

} // namespace aion::gameserver::model::gameobjects
