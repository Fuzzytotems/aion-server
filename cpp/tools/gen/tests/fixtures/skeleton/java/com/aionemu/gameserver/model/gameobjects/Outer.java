package com.aionemu.gameserver.model.gameobjects;

public class Outer {

	private final Inner first = new Inner();

	public Inner getFirst() {
		return first;
	}

	private static class Base {

		private Base(int value) {
		}
	}

	private static class Derived extends Base {

		private Derived() {
			super(1);
		}
	}

	public static class Inner extends Outer {

		public int size() {
			return 0;
		}
	}
}
