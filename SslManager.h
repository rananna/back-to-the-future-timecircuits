#ifndef SSL_MANAGER_H
#define SSL_MANAGER_H

#include <string>

namespace SslManager {

/**
 * @brief Initializes the SSL Manager.
 * @details This function should be called once during the setup phase of the application.
 * It reads the CA certificate bundle from "/cacert.pem" in the LittleFS filesystem
 * and stores it in a static, in-memory cache for efficient reuse.
 */
void begin();

/**
 * @brief Retrieves the cached CA certificate bundle.
 * @return A pointer to a null-terminated C-string containing the CA certificate bundle,
 *         or nullptr if the bundle has not been loaded or is empty. The pointer is
 *         valid for the lifetime of the application.
 */
const char* getCaCertBundle();

/**
 * @brief Checks if the CA certificate bundle was successfully loaded into memory.
 * @return True if the bundle is loaded and not empty, false otherwise.
 */
bool isLoaded();

/**
 * @brief Retrieves the size of the cached CA certificate bundle.
 * @return The size of the certificate bundle in bytes, including the null terminator.
 *         Returns 0 if the bundle is not loaded.
 */
size_t getCaCertBundleSize();

} // namespace SslManager

#endif // SSL_MANAGER_H
