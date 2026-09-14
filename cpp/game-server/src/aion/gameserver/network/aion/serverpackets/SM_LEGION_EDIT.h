#pragma once

#include <cstdint>
#include <string>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/model/team/legion/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author Simple
 */
class SM_LEGION_EDIT : public AionServerPacket {
private:
	int32_t type{};
	runtime::Ref<model::team::legion::Legion> legion{};
	int32_t unixTime{};
	std::string announcement{};
public:
	explicit SM_LEGION_EDIT(int32_t type);
	SM_LEGION_EDIT(int32_t type, model::team::legion::Legion& legion);
	SM_LEGION_EDIT(int32_t type, int32_t unixTime);
	explicit SM_LEGION_EDIT(model::team::legion::Legion::Announcement& announcement);
	~SM_LEGION_EDIT() override;
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
