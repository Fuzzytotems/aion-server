#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/team/legion/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * S0c declaration header (hub-headers.md §12). C++ difference: SM_GM_SHOW_LEGION_MEMBERLIST derives from this packet; Java takes the opcode
 * from the dynamic class, so the protected constructor takes the subclass's `opcodeOf<SM_X>`.
 *
 * @author Simple
 */
class SM_LEGION_MEMBERLIST : public AionServerPacket {
private:
	bool isFirst{};
	bool isLast{};
	std::vector<runtime::Ref<model::team::legion::LegionMember>> legionMembers{};
public:
	SM_LEGION_MEMBERLIST(const std::vector<runtime::Ptr<model::team::legion::LegionMember>>& legionMembers, bool isFirst, bool isLast);
	~SM_LEGION_MEMBERLIST() override;

protected:
	/** C++ only: the constructor of a subclass with its own opcode (Java: the opcode of getClass()) */
	SM_LEGION_MEMBERLIST(int32_t opCode, const std::vector<runtime::Ptr<model::team::legion::LegionMember>>& legionMembers, bool isFirst,
		bool isLast);

	void writeImpl(AionConnection* con) override;
	virtual void writeLegionMember(model::team::legion::LegionMember& legionMember);
};

} // namespace aion::gameserver::network::aion::serverpackets
