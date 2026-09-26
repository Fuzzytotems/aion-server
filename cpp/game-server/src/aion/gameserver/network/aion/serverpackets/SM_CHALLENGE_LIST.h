#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/challenge/fwd.h"
#include "aion/gameserver/model/templates/challenge/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author ViAl
 */
class SM_CHALLENGE_LIST : public AionServerPacket {
public:
	int32_t action{};
	int32_t ownerId{};
	model::templates::challenge::ChallengeType ownerType{};
	std::vector<runtime::Ref<model::challenge::ChallengeTask>> tasks{};
	runtime::Ref<model::challenge::ChallengeTask> task{};
	SM_CHALLENGE_LIST(int32_t action, int32_t ownerId, model::templates::challenge::ChallengeType ownerType,
		const std::vector<runtime::Ptr<model::challenge::ChallengeTask>>& tasks);
	SM_CHALLENGE_LIST(int32_t action, int32_t ownerId, model::templates::challenge::ChallengeType ownerType, model::challenge::ChallengeTask& task);
	~SM_CHALLENGE_LIST() override;

	/** writeImpl reads the connection: serialized per recipient (runtime-architecture.md §8.3) */
	Recipients recipients() const noexcept override { return Recipients::PER_RECIPIENT; }

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
