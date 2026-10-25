package com.narze.questbrowser;

/**
 * Allows plain HTTP for all hosts, and keeps the other rules of the app's policy.
 *
 * Beat Saber's network security config blocks plain HTTP, and WebView asks this
 * process-wide policy for each http:// request. Without this, local servers such
 * as http://nas.local:8096 do not load.
 */
final class PermissiveNetworkPolicy extends libcore.net.NetworkSecurityPolicy {
    private final libcore.net.NetworkSecurityPolicy base;

    PermissiveNetworkPolicy(libcore.net.NetworkSecurityPolicy base) {
        this.base = base;
    }

    @Override
    public boolean isCleartextTrafficPermitted() {
        return true;
    }

    @Override
    public boolean isCleartextTrafficPermitted(String hostname) {
        return true;
    }

    @Override
    public boolean isCertificateTransparencyVerificationRequired(String hostname) {
        return base != null && base.isCertificateTransparencyVerificationRequired(hostname);
    }
}
