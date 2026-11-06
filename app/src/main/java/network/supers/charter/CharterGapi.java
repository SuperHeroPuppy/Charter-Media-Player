package network.supers.charter;

final class CharterGapi {
    static {
        System.loadLibrary("charter_gapi");
    }

    static int[] palette() { return nativePalette(); }
    static int[] metrics(int widthDp) { return nativeMetrics(widthDp); }

    private static native int[] nativePalette();
    private static native int[] nativeMetrics(int widthDp);

    private CharterGapi() { }
}
