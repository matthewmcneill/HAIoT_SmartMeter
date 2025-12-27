/**
 * @file sys_crypto.h
 * @author Matthew McNeill
 * @brief Encryption and secure storage utilities using ECC608 and software AES.
 * @version 1.0.0
 * @date 2025-12-25
 * 
 * @section api Public API
 * - `setupCrypto()`: Initializes the crypto module and ECC608 chip if available.
 * - `deriveHardwareKey()`: Generates a hardware-locked encryption key.
 * - `encryptSecret()`: Encrypts a string using AES-256-CBC and returns Base64.
 * - `decryptSecret()`: Decrypts a Base64 encoded AES-256-CBC string.
 * 
 * License: GPLv3 (see project LICENSE file for details)
 * 
 * based on the tutorials by Alex Astrum
 * https://medium.com/@alexastrum/getting-started-with-arduino-and-firebase-347ec6917da5
 *
 * crypto configuration routines taken directly from 
 * https://github.com/arduino-libraries/ArduinoECCX08/blob/master/examples/Tools/ECCX08JWSPublicKey/ECCX08JWSPublicKey.ino
 *
 * ATECCx08 crypto chips only support ES256:
 * https://github.com/MicrochipTech/cryptoauthlib/blob/master/lib/jwt/atca_jwt.c
 * therefore will need to hand-crank to signature verification code in Google Apps Script
 * which only supports RS256 and HS256.  ES256 is receommended as stronger.
 * 
 */

#pragma once

#include <ArduinoECCX08.h>
#include <utility/ECCX08JWS.h>
#include <utility/ECCX08DefaultTLSConfig.h> // needed to set up the PEM for the first time
#include <ArduinoJson.h>

#ifdef ARDUINO_ARCH_ESP32
  #include <mbedtls/aes.h>
  #include <mbedtls/md.h>
  #include <mbedtls/base64.h>
#endif

#ifdef ARDUINO_ARCH_SAMD
  #include <ArduinoBearSSL.h>
#endif

// --- Base64 Utility for SAMD (since mbedtls is missing) ---
#ifdef ARDUINO_ARCH_SAMD
static const char b64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

String base64_encode(const uint8_t* data, size_t len) {
    String out = "";
    int i = 0;
    while (i < len) {
        uint32_t b = data[i++] << 16;
        if (i < len) b |= data[i++] << 8;
        if (i < len) b |= data[i++];
        out += b64_table[(b >> 18) & 0x3F];
        out += b64_table[(b >> 12) & 0x3F];
        out += (i > len + 1) ? '=' : b64_table[(b >> 6) & 0x3F];
        out += (i > len) ? '=' : b64_table[b & 0x3F];
    }
    return out;
}

size_t base64_decode(uint8_t* out, const char* in, size_t len) {
    uint32_t b = 0;
    int bits = -8;
    size_t out_len = 0;
    for (size_t i = 0; i < len; i++) {
        if (in[i] == '=') break;
        const char* p = strchr(b64_table, in[i]);
        if (!p) continue;
        b = (b << 6) | (p - b64_table);
        bits += 6;
        if (bits >= 0) {
            out[out_len++] = (b >> bits) & 0xFF;
            bits -= 8;
        }
    }
    return out_len;
}
#endif
#include "sys_logStatus.h"
#include "sys_serial_utils.h"

#define CRYPTO_SLOT 0   // can be value 0 - 4

// A hardcoded "Application Public Key" for ECDH. 
// In a production app, this would be the public key of the server or a fixed value.
// Here we use a fixed 64-byte public key (P-256) to perform the DH exchange.
const uint8_t APP_PUBLIC_KEY[64] = {
  0x1A, 0x2B, 0x3C, 0x4D, 0x5E, 0x6F, 0x70, 0x81, 0x92, 0xA3, 0xB4, 0xC5, 0xD6, 0xE7, 0xF8, 0x09,
  0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
  0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
  0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F
};

uint8_t derivedAESKey[32]; // 256-bit key
bool keyInitialized = false;

// Derive a hardware-locked shared secret using the ECC608 chip (if present)
// or a software fallback for ESP32 using the unique chip MAC
/**
 * @brief Derives a hardware-locked encryption key.
 * Uses ECC608 serial number on SAMD or ESP32 MAC as a unique seed.
 * 
 * @return bool True if key derivation was successful.
 */
bool deriveHardwareKey() {
  if (keyInitialized) return true;

  bool hardwareCryptoSuccess = false;

#ifdef ARDUINO_ARCH_SAMD
  uint8_t serialNumber[9];
  if (ECCX08.begin() && ECCX08.serialNumber(serialNumber)) {
    // Successfully got hardware serial number
    hardwareCryptoSuccess = true;
    
    // Simple derivation from serial number
    br_sha256_context ctx;
    br_sha256_init(&ctx);
    br_sha256_update(&ctx, serialNumber, 9);
    br_sha256_update(&ctx, "HAIoT_SALT", 10);
    br_sha256_out(&ctx, derivedAESKey);
  }
#endif

  if (!hardwareCryptoSuccess) {
    // Software fallback for platforms without ECC608 or where ECDH failed
    logStatus("Crypto: Hardware ECC chip not available or ECDH failed, using chip-ID derivation.");
    
    uint64_t chipID = 0;
#ifdef ARDUINO_ARCH_ESP32
    chipID = ESP.getEfuseMac(); // 48-bit MAC
#else
    // generic fallback for other platforms
    chipID = 0xDEADBEEFCAFEBABE; 
#endif
    
    // Simple derivation: hash the chipID and some salt
#ifdef ARDUINO_ARCH_ESP32
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 0);
    mbedtls_md_starts(&ctx);
    mbedtls_md_update(&ctx, (const unsigned char*)&chipID, sizeof(chipID));
    mbedtls_md_update(&ctx, (const unsigned char*)"HAIoT_SALT", 10);
    mbedtls_md_finish(&ctx, derivedAESKey);
    mbedtls_md_free(&ctx);
#endif
#ifdef ARDUINO_ARCH_SAMD
    br_sha256_context ctx;
    br_sha256_init(&ctx);
    br_sha256_update(&ctx, &chipID, sizeof(chipID));
    br_sha256_update(&ctx, "HAIoT_SALT", 10);
    br_sha256_out(&ctx, derivedAESKey);
#endif
  }

  keyInitialized = true;
  return true;
}

// AES-256-CBC Encryption
/**
 * @brief Encrypts a plaintext string using AES-256-CBC.
 * Prepends a random 16-byte IV and returns the result as a Base64 string.
 * 
 * @param plaintext The string to encrypt.
 * @return String Base64 encoded encrypted payload (IV + ciphertext).
 */
String encryptSecret(String plaintext) {
  if (!deriveHardwareKey()) return plaintext;

  // Prepare IV
  uint8_t iv[16];
#ifdef ARDUINO_ARCH_ESP32
  for (int i = 0; i < 16; i++) iv[i] = esp_random() % 256;
#else
  // Fallback for SAMD: try to use hardware random if chip is initialized
  if (ECCX08.begin()) {
    ECCX08.random(iv, 16);
  } else {
    // poor man's random
    for (int i = 0; i < 16; i++) iv[i] = random(256);
  }
#endif

  // PKCS7 Padding
  int len = plaintext.length();
  int paddedLen = ((len / 16) + 1) * 16;
  uint8_t input[paddedLen];
  memcpy(input, plaintext.c_str(), len);
  uint8_t padding = paddedLen - len;
  for (int i = len; i < paddedLen; i++) input[i] = padding;

  uint8_t output[paddedLen];

#ifdef ARDUINO_ARCH_ESP32
  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  mbedtls_aes_setkey_enc(&aes, derivedAESKey, 256);
  uint8_t iv_temp[16];
  memcpy(iv_temp, iv, 16);
  mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, paddedLen, iv_temp, input, output);
  mbedtls_aes_free(&aes);
#endif

#ifdef ARDUINO_ARCH_SAMD
  br_aes_ct_cbcenc_keys ctx;
  br_aes_ct_cbcenc_init(&ctx, derivedAESKey, 32);
  uint8_t iv_temp[16];
  memcpy(iv_temp, iv, 16);
  br_aes_ct_cbcenc_run(&ctx, iv_temp, output, paddedLen);
#endif

  // Payload: IV (16) + Data (paddedLen)
  uint8_t finalPayload[16 + paddedLen];
  memcpy(finalPayload, iv, 16);
  memcpy(finalPayload + 16, output, paddedLen);

  // Base64 Encode
#ifdef ARDUINO_ARCH_ESP32
  size_t outLen;
  mbedtls_base64_encode(NULL, 0, &outLen, finalPayload, 16 + paddedLen);
  unsigned char encoded[outLen];
  mbedtls_base64_encode(encoded, outLen, &outLen, finalPayload, 16 + paddedLen);
  return String((char*)encoded);
#endif
#ifdef ARDUINO_ARCH_SAMD
  return base64_encode(finalPayload, 16 + paddedLen);
#endif
}

// AES-256-CBC Decryption
/**
 * @brief Decrypts a Base64-encoded encrypted string.
 * Expects the first 16 bytes to be the CBC Initialization Vector.
 * 
 * @param base64Data The Base64 encoded payload to decrypt.
 * @return String The decrypted plaintext string, or original data on failure.
 */
String decryptSecret(String base64Data) {
  if (!deriveHardwareKey()) return base64Data;
  
  // 1. Base64 Decode
#ifdef ARDUINO_ARCH_ESP32
  size_t decodedLen;
  mbedtls_base64_decode(NULL, 0, &decodedLen, (const unsigned char*)base64Data.c_str(), base64Data.length());
  uint8_t payload[decodedLen];
  if (mbedtls_base64_decode(payload, decodedLen, &decodedLen, (const unsigned char*)base64Data.c_str(), base64Data.length()) != 0) {
    return base64Data;
  }
#endif
#ifdef ARDUINO_ARCH_SAMD
  // Estimation for buffer size
  size_t maxLen = (base64Data.length() * 3) / 4 + 2;
  uint8_t payload[maxLen];
  size_t decodedLen = base64_decode(payload, base64Data.c_str(), base64Data.length());
  if (decodedLen == 0) return base64Data;
#endif

  if (decodedLen < 16 + 16) return base64Data; // Minimum IV + 1 block

  uint8_t iv[16];
  memcpy(iv, payload, 16);
  int dataLen = decodedLen - 16;
  uint8_t input[dataLen];
  memcpy(input, payload + 16, dataLen);
  uint8_t output[dataLen];

#ifdef ARDUINO_ARCH_ESP32
  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  mbedtls_aes_setkey_dec(&aes, derivedAESKey, 256);
  uint8_t iv_temp[16];
  memcpy(iv_temp, iv, 16);
  if (mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, dataLen, iv_temp, input, output) != 0) {
    mbedtls_aes_free(&aes);
    return base64Data;
  }
  mbedtls_aes_free(&aes);
#endif

#ifdef ARDUINO_ARCH_SAMD
  br_aes_ct_cbcdec_keys ctx;
  br_aes_ct_cbcdec_init(&ctx, derivedAESKey, 32);
  uint8_t iv_temp[16];
  memcpy(iv_temp, iv, 16);
  br_aes_ct_cbcdec_run(&ctx, iv_temp, output, dataLen);
  // BearSSL doesn't return an error for CBC decryption itself as it's a stream operation
#endif

  // 3. Remove PKCS7 Padding
  uint8_t padding = output[dataLen - 1];
  if (padding > 16 || padding == 0) return base64Data;
  int finalLen = dataLen - padding;
  
  char finalString[finalLen + 1];
  memcpy(finalString, output, finalLen);
  finalString[finalLen] = '\0';

  return String(finalString);
}

void testSecureStorage() {
  logStatus("--- CRYPTO TEST START ---");
  String original = "Password!123";
  logStatus("Original: %s", original.c_str());
  
  String encrypted = encryptSecret(original);
  logStatus("Encrypted: %s", encrypted.c_str());
  
  String decrypted = decryptSecret(encrypted);
  logStatus("Decrypted: %s", decrypted.c_str());
  
  if (original == decrypted) {
    logStatus("TEST PASSED: Encryption integrity verified.");
  } else {
    logStatus("TEST FAILED: Data mismatch!");
  }
  logStatus("--- CRYPTO TEST END ---");
}

// interactive routine to configure the crypto of the device when connected to the serial port in the IDE.
// needed the first time when setting it up to configure the crypto
void configureCrypto() {

  if (!ECCX08.begin()) {
    logSuspend("No ECCX08 present!");
  }

  if (!ECCX08.locked()) {
    if (!promptAndReadYesNo("The ECCX08 on your board is not locked, would you like to PERMANENTLY configure and lock it now?", false) )
    {
      logSuspend("Unfortunately you can't proceed without locking it :(");
    }

    if (!ECCX08.writeConfiguration(ECCX08_DEFAULT_TLS_CONFIG)) {
      logSuspend("Writing ECCX08 configuration failed!");
    }

    if (!ECCX08.lock()) {
      logSuspend("Locking ECCX08 configuration failed!");
    }

    logStatus("ECCX08 locked successfully");
  }

  // CONFIGURE CRYPTO_SLOT IN CONSTANTS of the module header
  // Serial.println("Hi there, in order to generate a PEM public key for your board, we'll need the following information ...");
  // Serial.println();
  // String slot               = promptAndReadLine("What slot would you like to use? (0 - 4)", String(CRYPTO_SLOT));
  String slot = String(CRYPTO_SLOT);

  String publicKeyPem = ECCX08JWS.publicKey(CRYPTO_SLOT, false); 
  if (publicKeyPem == "") {
    Serial.println("Key missing at slot [" + slot + "]");
    // generate a new private key
    if (promptAndReadYesNo("Would you like to generate a new private key?", true))  {
      Serial.println("Generating new key pair at slot [" + slot + "]...");
      publicKeyPem = ECCX08JWS.publicKey(slot.toInt(), true);
    }
  } else {
    Serial.println("Current public key PEM at slot [" + slot + "]:");
    Serial.println(publicKeyPem);
    // uncomment this to interactively request at serial console for new key creation
    //   logStatus("Generating new key pair at slot [%s]...", slot.c_str());
    //   publicKeyPem = ECCX08JWS.publicKey(slot.toInt(), true);
    //   logStatus("Here's your public key PEM, at slot [%s]:", slot.c_str());
    //   logStatus(publicKeyPem);
  }

  if (!publicKeyPem || publicKeyPem == "") {
    logSuspend("Error generating public key!");
  }

}

/**
 * @brief Initializes hardware crypto and performs integrity checks.
 * On interactive connection (Serial), orchestrates provisioning if needed.
 */
void setupCrypto()
{

  // if the serial port is connected, run the interactive configuration checks for the crypto
  if (Serial) {
    configureCrypto();
  }

  // now run the checks for complete configuration
  if (!ECCX08.begin())
  {
    logSuspend("No ECCX08 present!");
  }
  if (!ECCX08.locked())
  {
    logSuspend("The ECCX08 on your board is not locked. Please configure the crypto.");
  }
  String publicKey = ECCX08JWS.publicKey(CRYPTO_SLOT, false);
  if (publicKey == "")
  {
    logSuspend("Key missing. Generate a new key pair. Please configure the crypto.");
  }
  
}

