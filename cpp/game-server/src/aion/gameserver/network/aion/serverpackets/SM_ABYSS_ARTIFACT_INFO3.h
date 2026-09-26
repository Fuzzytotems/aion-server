#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/siege/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

class SM_ABYSS_ARTIFACT_INFO3 : public AionServerPacket {
private:
	std::vector<runtime::Ref<model::siege::ArtifactLocation>> locations{};

public:
	explicit SM_ABYSS_ARTIFACT_INFO3(const std::vector<runtime::Ptr<model::siege::ArtifactLocation>>& collection);
	explicit SM_ABYSS_ARTIFACT_INFO3(int32_t loc);
	~SM_ABYSS_ARTIFACT_INFO3() override;

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
