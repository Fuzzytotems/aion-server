#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/team/legion/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * S0c declaration header (hub-headers.md §12). C++ difference: SM_GM_SHOW_LEGION_INFO derives from this packet; Java takes the opcode from the
 * dynamic class, so the protected constructor takes the subclass's `opcodeOf<SM_X>`.
 *
 * @author Simple
 */
class SM_LEGION_INFO : public AionServerPacket {
private:
	runtime::Ref<model::team::legion::Legion> legion{};
public:
	explicit SM_LEGION_INFO(model::team::legion::Legion& legion);
	~SM_LEGION_INFO() override;

protected:
	/** C++ only: the constructor of a subclass with its own opcode (Java: the opcode of getClass()) */
	SM_LEGION_INFO(int32_t opCode, model::team::legion::Legion& legion);

	void writeImpl(AionConnection* con) override;
private:
	/**
	 * The game client expects up to 7 announcements, but it only shows the first one, so only one is sent. The code could be simplified with just one
	 * announcement, but this implementation is more accurate and future-proof.
	 */
	void writeAnnouncements();
};

} // namespace aion::gameserver::network::aion::serverpackets
