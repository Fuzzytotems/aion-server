#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/fields/Array.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/house/fwd.h"

namespace aion::gameserver::model::house {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). Java `record PlayerScript(int id, byte[] compressedBytes, int uncompressedSize)`:
 * RefCounted (fieldmap K4, `PlayerScripts.scripts`), created with create(). The byte array is stored as it is (null for an empty script).
 * LUA_SANDBOX_FIX: Java's static initializer compresses the fix script with CompressUtil (zlib), which has no C++ port yet; the constant stays
 * null until it does (a static initializer must not reach AION_UNPORTED, hub-headers.md §2).
 *
 * @author Rolandas, Neon, Sykra
 */
class PlayerScript : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
public:
	/** mitigates https://appsec.space/posts/aion-housing-exploit/ (null until CompressUtil is ported, see the class comment) */
	static const runtime::Ref<PlayerScript> LUA_SANDBOX_FIX;

private:
	const int32_t id_;
	const runtime::Ref<runtime::Array<int8_t>> compressedBytes_;
	const int32_t uncompressedSize_;

protected:
	PlayerScript(int32_t id, runtime::Ptr<runtime::Array<int8_t>> compressedBytes, int32_t uncompressedSize);
	~PlayerScript() override;

public:
	/** Java: new PlayerScript(id, compressedBytes, uncompressedSize) (canonical record constructor) */
	static runtime::Ref<PlayerScript> create(int32_t id, runtime::Ptr<runtime::Array<int8_t>> compressedBytes, int32_t uncompressedSize);

	int32_t id() const { return id_; }

	runtime::Ptr<runtime::Array<int8_t>> compressedBytes() const { return compressedBytes_; }

	int32_t uncompressedSize() const { return uncompressedSize_; }

	bool hasData();

	/** Java record equals (the byte array component compares by identity) */
	bool equals(const PlayerScript& obj) const;

	/** Java record hashCode (the byte array component hashes by identity) */
	int32_t hashCode() const;
};

} // namespace aion::gameserver::model::house
