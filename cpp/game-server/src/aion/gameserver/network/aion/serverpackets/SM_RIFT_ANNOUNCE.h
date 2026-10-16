#pragma once

#include <cstdint>
#include <map>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/controllers/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author Sweetkr, -Enomine-
 */
class SM_RIFT_ANNOUNCE : public AionServerPacket {
private:
	int32_t actionId{};
	runtime::Ref<controllers::RVController> rift{};
	std::map<int32_t, int32_t> rifts{}; // fieldmap.toml: Java passes a TreeMap (RiftInformer.getAnnounceData), written in key order
	int32_t objectId{};
	int32_t gelkmaros{};
	int32_t inggison{};
public:
	explicit SM_RIFT_ANNOUNCE(const std::map<int32_t, int32_t>& rifts);
	SM_RIFT_ANNOUNCE(bool gelkmaros, bool inggison);
	SM_RIFT_ANNOUNCE(controllers::RVController& rift, bool isMaster);
	explicit SM_RIFT_ANNOUNCE(int32_t objectId);
	~SM_RIFT_ANNOUNCE() override;
protected:
	void writeImpl(AionConnection* con) override;
private:
	void writeRiftType();
};

} // namespace aion::gameserver::network::aion::serverpackets
