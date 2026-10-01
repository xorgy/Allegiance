#pragma once
#include <cstdint>
#include <span>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#else
#include <openssl/evp.h>
#include <openssl/rand.h>
#endif

namespace Native::Crypto {
// Use each platform's maintained crypto provider. Only content identity and
// reconnect tokens use this; GNS owns connection encryption/authentication.
class Sha256 {
public:
    Sha256() {
#ifdef _WIN32
        if(BCryptOpenAlgorithmProvider(&algorithm_,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0 ||
           BCryptCreateHash(algorithm_,&hash_,nullptr,0,nullptr,0,0)<0) {
            if(algorithm_)BCryptCloseAlgorithmProvider(algorithm_,0);
            throw std::runtime_error("Content digest initialization");
        }
#else
        hash_=EVP_MD_CTX_new();
        if(!hash_ || EVP_DigestInit_ex(hash_,EVP_sha256(),nullptr)!=1) {
            EVP_MD_CTX_free(hash_);throw std::runtime_error("Content digest initialization");
        }
#endif
    }
    ~Sha256() {
#ifdef _WIN32
        BCryptDestroyHash(hash_);BCryptCloseAlgorithmProvider(algorithm_,0);
#else
        EVP_MD_CTX_free(hash_);
#endif
    }
    Sha256(const Sha256&)=delete;
    Sha256& operator=(const Sha256&)=delete;
    void update(std::span<const uint8_t> bytes) {
#ifdef _WIN32
        if(bytes.size()>UINT32_MAX || BCryptHashData(hash_,const_cast<PUCHAR>(bytes.data()),ULONG(bytes.size()),0)<0)
#else
        if(EVP_DigestUpdate(hash_,bytes.data(),bytes.size())!=1)
#endif
            throw std::runtime_error("Content digest update");
    }
    void finish(std::span<uint8_t,32> digest) {
#ifdef _WIN32
        if(BCryptFinishHash(hash_,digest.data(),ULONG(digest.size()),0)<0)
#else
        unsigned length{};
        if(EVP_DigestFinal_ex(hash_,digest.data(),&length)!=1 || length!=digest.size())
#endif
            throw std::runtime_error("Content digest completion");
    }
private:
#ifdef _WIN32
    BCRYPT_ALG_HANDLE algorithm_{};BCRYPT_HASH_HANDLE hash_{};
#else
    EVP_MD_CTX* hash_{};
#endif
};
inline void random(std::span<uint8_t> bytes) {
#ifdef _WIN32
    if(bytes.size()>UINT32_MAX || BCryptGenRandom(nullptr,bytes.data(),ULONG(bytes.size()),BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0)
#else
    if(bytes.size()>INT32_MAX || RAND_bytes(bytes.data(),int(bytes.size()))!=1)
#endif
        throw std::runtime_error("Session token generation");
}
}
