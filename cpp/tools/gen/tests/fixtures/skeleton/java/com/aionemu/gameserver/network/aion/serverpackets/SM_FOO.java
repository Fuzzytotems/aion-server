package com.aionemu.gameserver.network.aion.serverpackets;

import java.util.List;

import com.aionemu.gameserver.model.templates.item.ItemTemplate;
import com.aionemu.gameserver.network.aion.AionConnection;
import com.aionemu.gameserver.network.aion.AionServerPacket;

/**
 * @author Tester
 */
public class SM_FOO extends AionServerPacket {

	private final int value;

	public SM_FOO(int value) {
		super(1);
		this.value = value;
	}

	public SM_FOO(List<ItemTemplate> items) {
		super(1);
		this.value = items.size();
	}

	@Override
	protected void writeImpl(AionConnection con) {
	}
}
