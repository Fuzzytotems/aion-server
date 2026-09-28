package com.aionemu.gameserver.network.aion.serverpackets;

import java.util.List;

import com.aionemu.gameserver.model.gameobjects.Creature;

public class SM_FOO_LIST extends SM_FOO {

	public SM_FOO_LIST(List<Creature> creatures, boolean last) {
		super(creatures.size());
	}

	public SM_FOO_LIST(boolean last, List<Creature> creatures) {
		super(creatures.size());
	}
}
