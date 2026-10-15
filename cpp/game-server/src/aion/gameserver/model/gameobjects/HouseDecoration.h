#pragma once

#include <cstdint>
#include <string>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/AionObject.h"
#include "aion/gameserver/model/gameobjects/Persistable.h"
#include "aion/gameserver/model/gameobjects/Persistable_PersistentState.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/templates/housing/fwd.h"

namespace aion::gameserver::model::gameobjects {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5), written for the member-type guards waiting for it at the spine freeze.
 *
 * @author Rolandas
 */
class HouseDecoration : public AionObject, public Persistable {
	AION_MAKE_REF_FRIEND
private:
	const int32_t templateId;
	runtime::Field<int8_t> room{};
	runtime::Field<Persistable::PersistentState> persistentState{};

protected:
	HouseDecoration(int32_t objectId, int32_t templateId);

public:
	static runtime::Ref<HouseDecoration> create(int32_t value, int32_t templateIdValue);

protected:
	HouseDecoration(int32_t objectId, int32_t templateId, int32_t room);

public:
	static runtime::Ref<HouseDecoration> create(int32_t value, int32_t templateIdValue, int32_t roomValue);

	int32_t getTemplateId() const { return this->templateId; }

	const templates::housing::HousePart* getTemplate();

	Persistable::PersistentState getPersistentState() override { return this->persistentState.get(); }

	void setPersistentState(Persistable::PersistentState persistentState) override;

	std::string getName() override;

	int8_t getRoom() const { return this->room.get(); }

	void setRoom(int32_t value);

protected:
	~HouseDecoration() override;
};

} // namespace aion::gameserver::model::gameobjects
