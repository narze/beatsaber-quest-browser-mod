package libcore.net;

/**
 * Compile-only stub of the Android internal class libcore.net.NetworkSecurityPolicy
 * (Android 9 to 14). It is not in the dex: at runtime the real class from the boot
 * class path is used. See cmake/dex.cmake.
 */
public abstract class NetworkSecurityPolicy {
    public NetworkSecurityPolicy() {}

    public static NetworkSecurityPolicy getInstance() {
        throw new UnsupportedOperationException("stub");
    }

    public static void setInstance(NetworkSecurityPolicy policy) {
        throw new UnsupportedOperationException("stub");
    }

    public abstract boolean isCleartextTrafficPermitted();

    public abstract boolean isCleartextTrafficPermitted(String hostname);

    public abstract boolean isCertificateTransparencyVerificationRequired(String hostname);
}
