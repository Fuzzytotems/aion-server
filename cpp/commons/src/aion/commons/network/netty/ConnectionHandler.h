#pragma once

#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <asio/steady_timer.hpp>

#include "aion/commons/network/AConnection.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::commons::network::netty {

class NettyConnectionBase;

/**
 * Turns received frames into packets of its connection and enforces the authentication timeout and the send stall limit.
 * <p>
 * C++: Netty's channel with the pipeline NettyPipeline sets up, as a commons connection (asio):
 * <ul>
 * <li><b>Frame decoder</b> (LengthFieldBasedFrameDecoder): the commons framing with a read buffer of the maximum packet size. A frame announcing
 * more than that, or less than its size header, is an invalid packet ("sent an invalid packet, disconnecting" with Netty's message), a frame
 * without body an empty packet; both disconnect.</li>
 * <li><b>Packet encoder</b>: packets are written by the sending thread into a buffer of the maximum packet size (see NettyConnection) and the
 * encoded bytes are queued; a packet that does not fit closes the connection ("Error in connection of ..."), like the encoder exception in
 * Netty.</li>
 * <li><b>Writability</b>: like Netty's default write buffer water marks, the channel becomes unwritable when more than 64 KiB wait to be written
 * to the socket and writable again below 32 KiB.</li>
 * <li><b>Closing</b> (channel.close()): the queued packets are dropped and the socket is closed once the bytes already handed to the socket
 * were written (at most 2 seconds later); data received after the close is not processed.</li>
 * </ul>
 * <p>
 * Ownership: the commons server keeps the handler alive while it is connected; the handler keeps its connection until it was disconnected and
 * then hands it to the disconnect task (so the connection, which keeps its channel, does not form a cycle with it).
 * <p>
 * Java: com.aionemu.commons.network.netty.ConnectionHandler (plus Netty's channel, frame decoder and PacketEncoder)
 */
class ConnectionHandler final : public AConnectionBase {
public:
	/** Netty's default WriteBufferWaterMark */
	static constexpr int64_t WRITE_BUFFER_LOW_WATER_MARK = 32 * 1024;
	static constexpr int64_t WRITE_BUFFER_HIGH_WATER_MARK = 64 * 1024;

	/**
	 * @param socket the accepted socket (SO_KEEPALIVE is enabled, TCP_NODELAY is set by the server)
	 * @param maxPacketSize the maximum size of a received and of a sent packet, including its size header
	 */
	ConnectionHandler(asio::ip::tcp::socket socket, NioServer& server, int32_t maxPacketSize);
	~ConnectionHandler() override;

	/** Attaches the connection (NettyPipeline::init), before the handler is returned to the server. */
	void setConnection(std::shared_ptr<NettyConnectionBase> connection);

	int32_t getMaxPacketSize() const noexcept { return maxPacketSize; }

	/** Java: channel.isActive() - false once the channel is closing or closed */
	bool isActive() const noexcept { return !isPendingClose() && !isClosed(); }

	/** Java: channel.isWritable() */
	bool isWritable() const;

	/** Java: channel.bytesBeforeWritable() - the bytes that must be written before the channel becomes writable again, 0 if it is writable */
	int64_t bytesBeforeWritable() const;

	/**
	 * Java: channel.writeAndFlush(packet) - encodes the packet with the writer and queues it; ignored if the channel is not active.
	 *
	 * @param closeAfterWrite true to close the channel once the packet was written (Java: addListener(ChannelFutureListener.CLOSE))
	 */
	void writeAndFlush(const std::function<void(utils::ByteBuffer&)>& writer, bool closeAfterWrite);

	/** Java: channel.close() / ctx.close() */
	void closeChannel();

	/** @return the connection's toString(), or "Client &lt;ip&gt;" if it has none (anymore) */
	std::string toString() const override;

protected:
	/** Java: channelActive */
	void initialized() override;
	/** Java: channelRead0 */
	bool processData(utils::ByteBuffer& data) override;
	bool writeData(utils::ByteBuffer& data) override;
	/** Java: channelInactive */
	void onDisconnect() override;
	/** Java: the channel group of NetConnector is closed on shutdown */
	void onServerClose() override;
	void onPacketTooLarge(int32_t size, int32_t capacity) const override;
	void onEmptyPacket(int32_t size, const utils::ByteBuffer& buf) const override;
	void onBytesWritten(size_t bytes) noexcept override;
	void onSocketClosed() noexcept override;

private:
	/** Java: exceptionCaught for a DecoderException */
	void invalidPacket(const std::string& message) const;
	/** Updates the writability after the pending bytes changed. The caller must hold guard. */
	void updateWritability();
	/** Java: channelWritabilityChanged. The caller must hold guard. */
	void channelWritabilityChanged();

	const int32_t maxPacketSize;
	/** set before the handler is registered, then only moved out by onDisconnect */
	std::shared_ptr<NettyConnectionBase> connection;
	/** for toString(), also after the disconnect */
	std::weak_ptr<NettyConnectionBase> connectionRef;

	/** encoded packets waiting to be written (guarded by guard) */
	std::deque<std::vector<uint8_t>> sendQueue;
	/** bytes queued or handed to the socket and not written yet (guarded by guard) */
	int64_t pendingWriteBytes = 0;
	/** guarded by guard */
	bool writable = true;
	/** number of writability changes (guarded by guard) */
	int32_t writabilityChanges = 0;

	asio::steady_timer authTimer; // strand only
	asio::steady_timer stallTimer; // strand only
};

} // namespace aion::commons::network::netty
