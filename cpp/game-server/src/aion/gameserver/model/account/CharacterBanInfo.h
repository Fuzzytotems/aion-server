#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/account/fwd.h"

namespace aion::gameserver::model::account {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). RefCounted (fieldmap K3, `PlayerAccountData.cbi`), created with create().
 *
 * @author nrg
 */
class CharacterBanInfo : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	const int64_t start;
	const int64_t end;
	const std::string reason;

protected:
	CharacterBanInfo(int64_t start, int64_t duration, std::string_view reason);
	~CharacterBanInfo() override;

public:
	/** Java: new CharacterBanInfo(start, duration, reason) */
	static runtime::Ref<CharacterBanInfo> create(int64_t start, int64_t duration, std::string_view reason);

	int64_t getStart() const { return start; }

	int64_t getEnd() const { return end; }

	std::string getReason() const { return reason; }
};

} // namespace aion::gameserver::model::account
