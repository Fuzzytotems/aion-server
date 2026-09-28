#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "aion/commons/database/SqlTypes.h"
#include "aion/gameserver/network/fwd.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"

namespace aion::gameserver::network {

/**
 * C++: RefCounted (fieldmap K4, held by BannedMacManager.bannedList); timeEnd is nullable like the Java Timestamp.
 *
 * @author KID
 */
class BannedMacEntry : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	const std::string mac;
	runtime::Field<std::string> details;
	// fieldmap.toml: Java compares timeEnd with null (isActive, isActiveTill): the nullable Timestamp field of hub-headers.md §6
	runtime::Field<std::optional<commons::database::Timestamp>> timeEnd{};

protected:
	BannedMacEntry(std::string_view address, int64_t newTime);

	BannedMacEntry(std::string_view address, std::optional<commons::database::Timestamp> time, std::string_view details);

	~BannedMacEntry() override;

public:
	/** Java: new BannedMacEntry(address, newTime) */
	static runtime::Ref<BannedMacEntry> create(std::string_view address, int64_t newTime);

	/** Java: new BannedMacEntry(address, time, details) */
	static runtime::Ref<BannedMacEntry> create(std::string_view address, std::optional<commons::database::Timestamp> time, std::string_view details);

	/** Java final */
	void setDetails(std::string_view value) { details.set(std::string(value)); }

	/** Java final */
	void updateTime(int64_t newTime);

	/** Java final */
	std::string getMac() const { return mac; }

	/** Java final */
	std::optional<commons::database::Timestamp> getTime() const { return timeEnd.get(); }

	/** Java final */
	bool isActive();

	/** Java final */
	bool isActiveTill(int64_t time);

	/** Java final */
	std::string getDetails() const { return details.get(); }
};

} // namespace aion::gameserver::network
