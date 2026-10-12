/**
 * @file      vm-version.h
 * @brief     Library version information and utilities.
 * @details   This header provides version information for the VM library,
 *            including version number components and macros for version comparison
 *            and string generation. It supports:
 *            - Major/Minor/Micro version components
 *            - Integer version encoding
 *            - String version generation
 *            
 * @author    Vladimir Shaleev <vladimirshaleev@gmail.com>
 * @ingroup   vm_files
 * @copyright MIT License
 */
#ifndef VM_VERSION_H
#define VM_VERSION_H

/**
 * @name  Version Components.
 * @brief Individual components of the library version.
 * @{
 */

/**
 * @brief   Major version number (API-breaking changes).
 * @sa      VM_VERSION
 * @sa      VM_VERSION_STRING
 * @ingroup vm_macros
 */
#define VM_VERSION_MAJOR 1

/**
 * @brief   Minor version number (backwards-compatible additions).
 * @sa      VM_VERSION
 * @sa      VM_VERSION_STRING
 * @ingroup vm_macros
 */
#define VM_VERSION_MINOR 0

/**
 * @brief   Micro version number (bug fixes and patches).
 * @sa      VM_VERSION
 * @sa      VM_VERSION_STRING
 * @ingroup vm_macros
 */
#define VM_VERSION_MICRO 0

/** @} */

/**
 * @name  Version Utilities.
 * @brief Macros for working with version numbers.
 * @{
 */

/**
 * @brief     Encodes version components into a single integer.
 * @details   Combines major, minor, and micro versions into a 32-bit value:
 *            - Bits 24-31: Major version (0-255)
 *            - Bits 16-23: Minor version (0-255)
 *            - Bits 0-15: Micro version (0-65535)
 *            The encoded value can be compared numerically when each component
 *            is within its documented range.
 * @param[in] major Major version number.
 * @param[in] minor Minor version number.
 * @param[in] micro Micro version number.
 * @return    Encoded version as unsigned long.
 * @sa        VM_VERSION
 * @ingroup   vm_macros
 */
#define VM_VERSION_ENCODE(major, minor, micro) \
    ((((unsigned long)(major)) << 24) | (((unsigned long)(minor)) << 16) | (unsigned long)(micro))

/**
 * @brief     Internal macro for string version generation.
 * @details   Helper macro that stringizes version components (e.g., 1, 0, 0 -> "1.0.0").
 * @param[in] major Major version number.
 * @param[in] minor Minor version number.
 * @param[in] micro Micro version number.
 * @return    Stringified version.
 * @note      For internal use only.
 * @ingroup   vm_macros
 * @private
 */
#define VM_VERSION_STRINGIZE_(major, minor, micro) #major "." #minor "." #micro

/**
 * @brief     Creates version string from components.
 * @details   Generates a string literal from version components (e.g., 1, 0, 0 -> "1.0.0").
 * @param[in] major Major version number.
 * @param[in] minor Minor version number.
 * @param[in] micro Micro version number.
 * @return    Stringified version.
 * @sa        VM_VERSION_STRING
 * @ingroup   vm_macros
 */
#define VM_VERSION_STRINGIZE(major, minor, micro)  VM_VERSION_STRINGIZE_(major, minor, micro)

/** @} */

/**
 * @name  Current Version.
 * @brief Macros representing the current library version.
 * @{
 */

/**
 * @brief   Encoded library version as integer.
 * @details Combined version value suitable for numeric comparisons.
 *          Use #VM_VERSION_STRING for human-readable format.
 * @sa      VM_VERSION_STRING
 * @ingroup vm_macros
 */
#define VM_VERSION VM_VERSION_ENCODE( \
    VM_VERSION_MAJOR, \
    VM_VERSION_MINOR, \
    VM_VERSION_MICRO)

/**
 * @brief   Library version as human-readable string.
 * @details Version string in "MAJOR.MINOR.MICRO" format (e.g., "1.0.0").
 *          Use #VM_VERSION for numeric comparisons.
 * @sa      VM_VERSION
 * @ingroup vm_macros
 */
#define VM_VERSION_STRING VM_VERSION_STRINGIZE( \
    VM_VERSION_MAJOR, \
    VM_VERSION_MINOR, \
    VM_VERSION_MICRO)

/** @} */

#endif /* VM_VERSION_H */
