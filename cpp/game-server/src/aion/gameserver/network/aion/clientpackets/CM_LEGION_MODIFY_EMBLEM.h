#pragma once

#include <cstdint>

#include "aion/gameserver/model/team/legion/LegionEmblemType.h"

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * A predefined emblem chosen by the brigade general (C_CHANGE_EMBLEM_VER).
 * <p>
 * C++ only: `CM_LEGION_MODIFY_EMBLEMTestAccess` (tests/cm_lz) reads the fields readImpl decoded, which Java keeps private without getters.
 *
 * @author Simple, cura, Neon
 */
class CM_LEGION_MODIFY_EMBLEM : public AionClientPacket {
	friend struct CM_LEGION_MODIFY_EMBLEMTestAccess;

private:
	int32_t legionId{};
	int32_t emblemId{};
	int32_t alpha{};
	int32_t red{};
	int32_t green{};
	int32_t blue{};
	model::team::legion::LegionEmblemType emblemType{};

public:
	CM_LEGION_MODIFY_EMBLEM(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
