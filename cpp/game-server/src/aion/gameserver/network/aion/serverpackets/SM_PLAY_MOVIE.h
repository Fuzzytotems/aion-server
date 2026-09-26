#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author -orz-, MrPoke
 */
class SM_PLAY_MOVIE : public AionServerPacket {
private:
	bool isMovie{};
	int32_t objectId{};
	int32_t questId{};
	int32_t cutsceneId{};
	bool canSkip{};
public:
	SM_PLAY_MOVIE(bool isCutsceneMovie, int32_t objectId, int32_t questId, int32_t cutsceneId, bool canSkip);

	/** C++ only: writeImpl reads the connection, so every recipient gets its own serialization (runtime-architecture.md §8.3) */
	Recipients recipients() const noexcept override { return Recipients::PER_RECIPIENT; }
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
