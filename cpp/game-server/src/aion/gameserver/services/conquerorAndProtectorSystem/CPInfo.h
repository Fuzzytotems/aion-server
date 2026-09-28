#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/templates/cp/fwd.h"
#include "aion/gameserver/services/conquerorAndProtectorSystem/fwd.h"

namespace aion::gameserver::services::conquerorAndProtectorSystem {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5), written for the member-type guards waiting for it at the spine freeze.
 *
 * @author Source
 */
class CPInfo : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	const model::templates::cp::CPType type;
	const int32_t playerId;
	runtime::Field<int32_t> rank{};
	runtime::Field<int32_t> ldRank{};
	runtime::Field<int32_t> victims{};
	const runtime::Ref<CPBuff> buff;

protected:
	CPInfo(model::templates::cp::CPType type, model::gameobjects::player::Player& owner);

public:
	static runtime::Ref<CPInfo> create(model::templates::cp::CPType value, model::gameobjects::player::Player& owner);

	model::templates::cp::CPType getType() const { return this->type; }

	int32_t getPlayerId() const { return this->playerId; }

	void setRank(int32_t value) { this->rank.set(value); }

	void setLDRank(int32_t value) { this->ldRank.set(value); }

	int32_t getRank() const { return this->rank.get(); }

	int32_t getLDRank() const { return this->ldRank.get(); }

	int32_t getVictims() const { return this->victims.get(); }

	void setVictims(int32_t value) { this->victims.set(value); }

	runtime::Ptr<CPBuff> getBuff() const { return this->buff; }

protected:
	~CPInfo() override;
};

} // namespace aion::gameserver::services::conquerorAndProtectorSystem
