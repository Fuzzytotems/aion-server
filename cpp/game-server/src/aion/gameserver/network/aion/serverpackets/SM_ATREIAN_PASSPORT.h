#pragma once

#include <cstdint>

#include "aion/commons/database/SqlTypes.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/account/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author ViAl, Neon
 */
class SM_ATREIAN_PASSPORT : public AionServerPacket {
private:
	commons::database::Date accountCreationDate{};
	runtime::Ref<model::account::PassportsList> passports{};
	int32_t stamps{};

public:
	SM_ATREIAN_PASSPORT(model::account::PassportsList& passports, int32_t stamps, commons::database::Date accountCreationDate);
	~SM_ATREIAN_PASSPORT() override;

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
