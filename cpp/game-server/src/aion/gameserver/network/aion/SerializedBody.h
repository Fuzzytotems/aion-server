#pragma once

#include <cstdint>
#include <memory>
#include <vector>

namespace aion::gameserver::network::aion {

/**
 * C++ only (runtime-architecture.md §8.1, §8.2): one server packet serialized eagerly on the sending thread by AionServerPacket::serialize, the
 * element type of AionConnection's send queue.
 * <p>
 * `bytes` hold the unencrypted packet body as Java's AionServerPacket.write produced it after the frame length: the obfuscated opcode, the static
 * server packet code, the inverted opcode and the writeImpl data. The IO strand (AionConnection::writeData) prepends the length and encrypts, so
 * crypt state stays strand-only (§8.6). The bytes are shared: a SHARED packet broadcast to many connections is serialized once (§8.3).
 * <p>
 * Thread-safety: immutable after serialize returns; copies share the bytes.
 */
struct SerializedBody {
	/** body bytes (never null for a serialized packet) */
	std::shared_ptr<const std::vector<uint8_t>> bytes;
	/** serialization sequence number sampled when serialization started; connection queues are ordered by it (§8.4) */
	uint64_t seq = 0;
	/** packet opcode (unobfuscated), for logs and RunnableStatsManager */
	int32_t opCode = 0;
	/** SM_KEY: the IO strand enables the crypt key after writing this packet unencrypted (§8.5) */
	bool enablesCrypt = false;
};

} // namespace aion::gameserver::network::aion
