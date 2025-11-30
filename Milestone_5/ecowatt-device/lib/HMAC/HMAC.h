#pragma once
#include <Crypto.h>
#include <Arduino.h>

/*
 * Lightweight HMAC implementation compatible with:
 *     HMAC<SHA256> hmac;
 *     hmac.reset(key, keylen);
 *     hmac.update(data, len);
 *     hmac.finalize(out, outlen);
 *
 * Works with rweather/Crypto (ESP8266 / ESP32 / Arduino)
 */

template <class Hash>
class HMAC {
public:
    HMAC() {}

    void reset(const uint8_t *key, size_t keyLen) {
        memset(innerKeyPad, 0x36, blockSize);
        memset(outerKeyPad, 0x5C, blockSize);

        if (keyLen > blockSize) {
            Hash temp;
            temp.reset();
            temp.update(key, keyLen);
            temp.finalize(keyBlock, sizeof(keyBlock));
            keyLen = Hash::HASH_SIZE;
        } else {
            memcpy(keyBlock, key, keyLen);
        }

        for (size_t i = 0; i < keyLen; ++i) {
            innerKeyPad[i] ^= keyBlock[i];
            outerKeyPad[i] ^= keyBlock[i];
        }

        inner.reset();
        inner.update(innerKeyPad, blockSize);

        outer.reset();
        outer.update(outerKeyPad, blockSize);
    }

    void update(const uint8_t *data, size_t len) {
        inner.update(data, len);
    }

    void finalize(uint8_t *out, size_t outLen) {
        uint8_t innerHash[Hash::HASH_SIZE];
        inner.finalize(innerHash, sizeof(innerHash));
        outer.update(innerHash, sizeof(innerHash));
        outer.finalize(out, outLen);
    }

private:
    static constexpr size_t blockSize = 64;
    uint8_t innerKeyPad[blockSize];
    uint8_t outerKeyPad[blockSize];
    uint8_t keyBlock[Hash::HASH_SIZE];
    Hash inner;
    Hash outer;
};
