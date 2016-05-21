package com.example.taskmon;

public class JniWrapper {
	static {
        System.loadLibrary("taskmon-jni");
	}
	
	public static native String stringFromJNI();
	
	public static native int[] getRelTID();
	
	public static native int setReserve(int tid, int C, int T, int cpuID);
	
	public static native int cancelReserve(int tid);
	
	public static native int[] getReservedTID();
	
	public static native int taskmonEnable();
	
	public static native int taskmonDisable();
	
	public static native int getFreq();
	
	public static native int getPower();
	
	public static native int getEnergy();
	
	public static native int getTidEnergy(int tid);
	
	public static native String getUtil(int tid);
}
