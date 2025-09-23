#include "SslManager.h"
#include <LittleFS.h>
#include "DebugLog.h"

namespace SslManager {

// A static string to cache the CA certificate bundle in memory.
// This avoids reading the file from flash for every single network request.
static std::string ca_cert_bundle;
static bool bundle_loaded = false;

void begin() {
    Log_printf(LOG_LEVEL_INFO, "Initializing SSL Manager...");
    if (!LittleFS.exists("/cacert.pem")) {
        Log_printf(LOG_LEVEL_WARN, "CA certificate bundle not found at /cacert.pem. Secure connections may fail.");
        return;
    }

    File certFile = LittleFS.open("/cacert.pem", "r");
    if (!certFile) {
        Log_printf(LOG_LEVEL_ERROR, "Failed to open /cacert.pem, even though it exists.");
        return;
    }

    if (certFile.size() == 0) {
        Log_printf(LOG_LEVEL_WARN, "/cacert.pem is empty. Secure connections will likely fail.");
        certFile.close();
        return;
    }

    // Pre-allocate memory for the string to avoid multiple reallocations.
    ca_cert_bundle.reserve(certFile.size());

    // Read the file directly into the string's buffer.
    ca_cert_bundle.assign((std::istreambuf_iterator<char>(certFile)), std::istreambuf_iterator<char>());

    certFile.close();

    if (!ca_cert_bundle.empty()) {
        bundle_loaded = true;
        Log_printf(LOG_LEVEL_INFO, "Successfully loaded CA certificate bundle (%d bytes).", ca_cert_bundle.length());
    } else {
        Log_printf(LOG_LEVEL_ERROR, "Failed to read CA certificate bundle from file, though it was not empty.");
    }
}

bool isLoaded() {
    return bundle_loaded;
}

const char* getCaCertBundle() {
    if (ca_cert_bundle.empty()) {
        return nullptr;
    }
    return ca_cert_bundle.c_str();
}

size_t getCaCertBundleSize() {
    if (ca_cert_bundle.empty()) {
        return 0;
    }
    // Return size including the null terminator, as required by some TLS libraries.
    return ca_cert_bundle.length() + 1;
}

} // namespace SslManager
