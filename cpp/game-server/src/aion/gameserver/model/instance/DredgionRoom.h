#pragma once

#include <cstdint>

#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"

namespace aion::gameserver::model::instance {

/**
 * Java com.aionemu.gameserver.model.instance.DredgionRoom (fieldmap: K4 SHARED, RefCounted; a member of DredgionScoreWriter.dredgionRooms).
 * 1 Primary Armory 2 Backup Armory 3 Gravity Control 4 Engine Room 5 Auxiliary Power 6 Weapons Deck 7 Lower Weapons Deck 8 Ready Room 1 9 Ready
 * Room 2 10 Barracks 11 Logistics Managment 12 Logistics Storage 13 The Bridge 14 Captain's Room
 */
class DredgionRoom : public runtime::RefCounted {
private:
	const int32_t roomId;
	runtime::Field<int32_t> state{0xFF};

public:
	explicit DredgionRoom(int32_t roomId);
	~DredgionRoom() override;

	static runtime::Ref<DredgionRoom> create(int32_t roomId);

	int32_t getRoomId() const;

	void captureRoom(Race race);

	int32_t getState() const;
};

} // namespace aion::gameserver::model::instance
