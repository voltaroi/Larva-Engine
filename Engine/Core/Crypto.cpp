#ifdef _WIN32
#define _CRT_RAND_S
#endif
#include "Crypto.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include "Sha256.h"

namespace larva
{
    namespace crypto
    {
        // ------------------------------------------------------------------
        //  Aléa
        // ------------------------------------------------------------------
        void randomBytes(void *out, size_t len)
        {
            uint8_t *p = (uint8_t *)out;
#ifdef _WIN32
            while (len > 0)
            {
                unsigned int v = 0;
                if (rand_s(&v) != 0)
                    throw std::runtime_error("rand_s");
                size_t n = len < 4 ? len : 4;
                std::memcpy(p, &v, n);
                p += n;
                len -= n;
            }
#else
            FILE *f = std::fopen("/dev/urandom", "rb");
            if (!f || std::fread(p, 1, len, f) != len)
                throw std::runtime_error("/dev/urandom");
            std::fclose(f);
#endif
        }

        // ------------------------------------------------------------------
        //  X25519 (arithmétique en 16 membres de 16 bits, à la TweetNaCl)
        // ------------------------------------------------------------------
        namespace
        {
            typedef int64_t gf[16];
            const gf K121665 = {0xDB41, 1};

            void car(gf o)
            {
                for (int i = 0; i < 16; ++i)
                {
                    o[i] += (int64_t)1 << 16;
                    int64_t c = o[i] >> 16;
                    o[(i + 1) * (i < 15)] += c - 1 + 37 * (c - 1) * (i == 15);
                    o[i] -= c * 65536;
                }
            }

            void sel(gf p, gf q, int b)
            {
                int64_t c = ~(int64_t)(b - 1);
                for (int i = 0; i < 16; ++i)
                {
                    int64_t t = c & (p[i] ^ q[i]);
                    p[i] ^= t;
                    q[i] ^= t;
                }
            }

            void pack(uint8_t *o, const gf n)
            {
                gf m, t;
                for (int i = 0; i < 16; ++i)
                    t[i] = n[i];
                car(t);
                car(t);
                car(t);
                for (int j = 0; j < 2; ++j)
                {
                    m[0] = t[0] - 0xffed;
                    for (int i = 1; i < 15; ++i)
                    {
                        m[i] = t[i] - 0xffff - ((m[i - 1] >> 16) & 1);
                        m[i - 1] &= 0xffff;
                    }
                    m[15] = t[15] - 0x7fff - ((m[14] >> 16) & 1);
                    int b = (int)((m[15] >> 16) & 1);
                    m[14] &= 0xffff;
                    sel(t, m, 1 - b);
                }
                for (int i = 0; i < 16; ++i)
                {
                    o[2 * i] = (uint8_t)(t[i] & 0xff);
                    o[2 * i + 1] = (uint8_t)(t[i] >> 8);
                }
            }

            void unpack(gf o, const uint8_t *n)
            {
                for (int i = 0; i < 16; ++i)
                    o[i] = n[2 * i] + ((int64_t)n[2 * i + 1] << 8);
                o[15] &= 0x7fff;
            }

            void add(gf o, const gf a, const gf b)
            {
                for (int i = 0; i < 16; ++i)
                    o[i] = a[i] + b[i];
            }
            void sub(gf o, const gf a, const gf b)
            {
                for (int i = 0; i < 16; ++i)
                    o[i] = a[i] - b[i];
            }
            void mul(gf o, const gf a, const gf b)
            {
                int64_t t[31] = {0};
                for (int i = 0; i < 16; ++i)
                    for (int j = 0; j < 16; ++j)
                        t[i + j] += a[i] * b[j];
                for (int i = 0; i < 15; ++i)
                    t[i] += 38 * t[i + 16];
                for (int i = 0; i < 16; ++i)
                    o[i] = t[i];
                car(o);
                car(o);
            }
            void sqr(gf o, const gf a) { mul(o, a, a); }
            void inv(gf o, const gf in)
            {
                gf c;
                for (int i = 0; i < 16; ++i)
                    c[i] = in[i];
                for (int a = 253; a >= 0; --a)
                {
                    sqr(c, c);
                    if (a != 2 && a != 4)
                        mul(c, c, in);
                }
                for (int i = 0; i < 16; ++i)
                    o[i] = c[i];
            }

            void scalarmult(uint8_t *q, const uint8_t *n, const uint8_t *p)
            {
                uint8_t z[32];
                gf x, a, b, c, d, e, f;
                for (int i = 0; i < 31; ++i)
                    z[i] = n[i];
                z[31] = (n[31] & 127) | 64;
                z[0] &= 248;
                unpack(x, p);
                for (int i = 0; i < 16; ++i)
                {
                    b[i] = x[i];
                    d[i] = a[i] = c[i] = 0;
                }
                a[0] = d[0] = 1;
                for (int i = 254; i >= 0; --i)
                {
                    int r = (z[i >> 3] >> (i & 7)) & 1;
                    sel(a, b, r);
                    sel(c, d, r);
                    add(e, a, c);
                    sub(a, a, c);
                    add(c, b, d);
                    sub(b, b, d);
                    sqr(d, e);
                    sqr(f, a);
                    mul(a, c, a);
                    mul(c, b, e);
                    add(e, a, c);
                    sub(a, a, c);
                    sqr(b, a);
                    sub(c, d, f);
                    mul(a, c, K121665);
                    add(a, a, d);
                    mul(c, c, a);
                    mul(a, d, f);
                    mul(d, b, x);
                    sqr(b, e);
                    sel(a, b, r);
                    sel(c, d, r);
                }
                inv(c, c);
                mul(a, a, c);
                pack(q, a);
            }
        }

        void x25519Public(uint8_t pub[32], const uint8_t priv[32])
        {
            uint8_t base[32] = {9};
            scalarmult(pub, priv, base);
        }

        void x25519Keypair(uint8_t priv[32], uint8_t pub[32])
        {
            randomBytes(priv, 32);
            x25519Public(pub, priv);
        }

        bool x25519(uint8_t out[32], const uint8_t priv[32], const uint8_t peerPub[32])
        {
            scalarmult(out, priv, peerPub);
            uint8_t acc = 0;
            for (int i = 0; i < 32; ++i)
                acc |= out[i];
            return acc != 0;
        }

        // ------------------------------------------------------------------
        //  ChaCha20
        // ------------------------------------------------------------------
        namespace
        {
            inline uint32_t rd32(const uint8_t *p) { return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
            inline void wr32(uint8_t *p, uint32_t v)
            {
                p[0] = (uint8_t)v;
                p[1] = (uint8_t)(v >> 8);
                p[2] = (uint8_t)(v >> 16);
                p[3] = (uint8_t)(v >> 24);
            }
            inline uint32_t rotl(uint32_t v, int n) { return (v << n) | (v >> (32 - n)); }
#define QR(a, b, c, d)                    \
    a += b, d ^= a, d = rotl(d, 16);       \
    c += d, b ^= c, b = rotl(b, 12);       \
    a += b, d ^= a, d = rotl(d, 8);        \
    c += d, b ^= c, b = rotl(b, 7)

            void chachaBlock(uint8_t out[64], const uint8_t key[32], uint32_t counter, const uint8_t nonce[12])
            {
                uint32_t s[16] = {0x61707865, 0x3320646e, 0x79622d32, 0x6b206574};
                for (int i = 0; i < 8; ++i)
                    s[4 + i] = rd32(key + 4 * i);
                s[12] = counter;
                for (int i = 0; i < 3; ++i)
                    s[13 + i] = rd32(nonce + 4 * i);
                uint32_t x[16];
                std::memcpy(x, s, sizeof(x));
                for (int i = 0; i < 10; ++i)
                {
                    QR(x[0], x[4], x[8], x[12]);
                    QR(x[1], x[5], x[9], x[13]);
                    QR(x[2], x[6], x[10], x[14]);
                    QR(x[3], x[7], x[11], x[15]);
                    QR(x[0], x[5], x[10], x[15]);
                    QR(x[1], x[6], x[11], x[12]);
                    QR(x[2], x[7], x[8], x[13]);
                    QR(x[3], x[4], x[9], x[14]);
                }
                for (int i = 0; i < 16; ++i)
                    wr32(out + 4 * i, x[i] + s[i]);
            }
#undef QR

            void chachaXor(const uint8_t key[32], uint32_t counter, const uint8_t nonce[12], const uint8_t *in, size_t len, uint8_t *out)
            {
                uint8_t block[64];
                while (len > 0)
                {
                    chachaBlock(block, key, counter++, nonce);
                    size_t n = len < 64 ? len : 64;
                    for (size_t i = 0; i < n; ++i)
                        out[i] = in[i] ^ block[i];
                    in += n;
                    out += n;
                    len -= n;
                }
            }
        }

        // ------------------------------------------------------------------
        //  Poly1305 (membres de 26 bits, à la poly1305-donna)
        // ------------------------------------------------------------------
        namespace
        {
            struct Poly
            {
                uint32_t r[5], h[5] = {0, 0, 0, 0, 0}, pad[4];
                uint8_t buf[16];
                size_t used = 0;

                explicit Poly(const uint8_t key[32])
                {
                    r[0] = (rd32(key + 0)) & 0x3ffffff;
                    r[1] = (rd32(key + 3) >> 2) & 0x3ffff03;
                    r[2] = (rd32(key + 6) >> 4) & 0x3ffc0ff;
                    r[3] = (rd32(key + 9) >> 6) & 0x3f03fff;
                    r[4] = (rd32(key + 12) >> 8) & 0x00fffff;
                    for (int i = 0; i < 4; ++i)
                        pad[i] = rd32(key + 16 + 4 * i);
                }

                void block(const uint8_t *m, uint32_t hibit)
                {
                    const uint32_t r0 = r[0], r1 = r[1], r2 = r[2], r3 = r[3], r4 = r[4];
                    const uint32_t s1 = r1 * 5, s2 = r2 * 5, s3 = r3 * 5, s4 = r4 * 5;
                    uint32_t h0 = h[0], h1 = h[1], h2 = h[2], h3 = h[3], h4 = h[4];
                    h0 += (rd32(m + 0)) & 0x3ffffff;
                    h1 += (rd32(m + 3) >> 2) & 0x3ffffff;
                    h2 += (rd32(m + 6) >> 4) & 0x3ffffff;
                    h3 += (rd32(m + 9) >> 6) & 0x3ffffff;
                    h4 += (rd32(m + 12) >> 8) | hibit;
                    uint64_t d0 = (uint64_t)h0 * r0 + (uint64_t)h1 * s4 + (uint64_t)h2 * s3 + (uint64_t)h3 * s2 + (uint64_t)h4 * s1;
                    uint64_t d1 = (uint64_t)h0 * r1 + (uint64_t)h1 * r0 + (uint64_t)h2 * s4 + (uint64_t)h3 * s3 + (uint64_t)h4 * s2;
                    uint64_t d2 = (uint64_t)h0 * r2 + (uint64_t)h1 * r1 + (uint64_t)h2 * r0 + (uint64_t)h3 * s4 + (uint64_t)h4 * s3;
                    uint64_t d3 = (uint64_t)h0 * r3 + (uint64_t)h1 * r2 + (uint64_t)h2 * r1 + (uint64_t)h3 * r0 + (uint64_t)h4 * s4;
                    uint64_t d4 = (uint64_t)h0 * r4 + (uint64_t)h1 * r3 + (uint64_t)h2 * r2 + (uint64_t)h3 * r1 + (uint64_t)h4 * r0;
                    uint32_t c = (uint32_t)(d0 >> 26);
                    h0 = (uint32_t)d0 & 0x3ffffff;
                    d1 += c;
                    c = (uint32_t)(d1 >> 26);
                    h1 = (uint32_t)d1 & 0x3ffffff;
                    d2 += c;
                    c = (uint32_t)(d2 >> 26);
                    h2 = (uint32_t)d2 & 0x3ffffff;
                    d3 += c;
                    c = (uint32_t)(d3 >> 26);
                    h3 = (uint32_t)d3 & 0x3ffffff;
                    d4 += c;
                    c = (uint32_t)(d4 >> 26);
                    h4 = (uint32_t)d4 & 0x3ffffff;
                    h0 += c * 5;
                    c = h0 >> 26;
                    h0 &= 0x3ffffff;
                    h1 += c;
                    h[0] = h0, h[1] = h1, h[2] = h2, h[3] = h3, h[4] = h4;
                }

                void update(const uint8_t *m, size_t len)
                {
                    while (len > 0)
                    {
                        if (used == 0 && len >= 16)
                        {
                            block(m, 1u << 24);
                            m += 16;
                            len -= 16;
                            continue;
                        }
                        size_t n = 16 - used < len ? 16 - used : len;
                        std::memcpy(buf + used, m, n);
                        used += n;
                        m += n;
                        len -= n;
                        if (used == 16)
                        {
                            block(buf, 1u << 24);
                            used = 0;
                        }
                    }
                }

                void finish(uint8_t tag[16])
                {
                    if (used)
                    {
                        buf[used] = 1;
                        for (size_t i = used + 1; i < 16; ++i)
                            buf[i] = 0;
                        block(buf, 0);
                    }
                    uint32_t h0 = h[0], h1 = h[1], h2 = h[2], h3 = h[3], h4 = h[4];
                    uint32_t c = h1 >> 26;
                    h1 &= 0x3ffffff;
                    h2 += c;
                    c = h2 >> 26;
                    h2 &= 0x3ffffff;
                    h3 += c;
                    c = h3 >> 26;
                    h3 &= 0x3ffffff;
                    h4 += c;
                    c = h4 >> 26;
                    h4 &= 0x3ffffff;
                    h0 += c * 5;
                    c = h0 >> 26;
                    h0 &= 0x3ffffff;
                    h1 += c;
                    uint32_t g0 = h0 + 5;
                    c = g0 >> 26;
                    g0 &= 0x3ffffff;
                    uint32_t g1 = h1 + c;
                    c = g1 >> 26;
                    g1 &= 0x3ffffff;
                    uint32_t g2 = h2 + c;
                    c = g2 >> 26;
                    g2 &= 0x3ffffff;
                    uint32_t g3 = h3 + c;
                    c = g3 >> 26;
                    g3 &= 0x3ffffff;
                    uint32_t g4 = h4 + c - (1u << 26);
                    uint32_t mask = (g4 >> 31) - 1;
                    g0 &= mask, g1 &= mask, g2 &= mask, g3 &= mask, g4 &= mask;
                    mask = ~mask;
                    h0 = (h0 & mask) | g0;
                    h1 = (h1 & mask) | g1;
                    h2 = (h2 & mask) | g2;
                    h3 = (h3 & mask) | g3;
                    h4 = (h4 & mask) | g4;
                    h0 = (h0 | (h1 << 26));
                    h1 = ((h1 >> 6) | (h2 << 20));
                    h2 = ((h2 >> 12) | (h3 << 14));
                    h3 = ((h3 >> 18) | (h4 << 8));
                    uint64_t f = (uint64_t)h0 + pad[0];
                    h0 = (uint32_t)f;
                    f = (uint64_t)h1 + pad[1] + (f >> 32);
                    h1 = (uint32_t)f;
                    f = (uint64_t)h2 + pad[2] + (f >> 32);
                    h2 = (uint32_t)f;
                    f = (uint64_t)h3 + pad[3] + (f >> 32);
                    h3 = (uint32_t)f;
                    wr32(tag + 0, h0);
                    wr32(tag + 4, h1);
                    wr32(tag + 8, h2);
                    wr32(tag + 12, h3);
                }
            };

            void aeadTag(uint8_t tag[16], const uint8_t key[32], const uint8_t nonce[12], const uint8_t *aad, size_t aadLen,
                         const uint8_t *ct, size_t ctLen)
            {
                uint8_t block[64];
                chachaBlock(block, key, 0, nonce);
                Poly p(block);
                static const uint8_t zeros[16] = {0};
                p.update(aad, aadLen);
                p.update(zeros, (16 - aadLen % 16) % 16);
                p.update(ct, ctLen);
                p.update(zeros, (16 - ctLen % 16) % 16);
                uint8_t lens[16];
                for (int i = 0; i < 8; ++i)
                {
                    lens[i] = (uint8_t)((uint64_t)aadLen >> (8 * i));
                    lens[8 + i] = (uint8_t)((uint64_t)ctLen >> (8 * i));
                }
                p.update(lens, 16);
                p.finish(tag);
            }

            void nonceOf(uint8_t n[12], uint64_t counter)
            {
                std::memset(n, 0, 4);
                for (int i = 0; i < 8; ++i)
                    n[4 + i] = (uint8_t)(counter >> (8 * i));
            }
        }

        void poly1305(uint8_t tag[16], const void *msg, size_t len, const uint8_t key[32])
        {
            Poly p(key);
            p.update((const uint8_t *)msg, len);
            p.finish(tag);
        }

        void seal12(const uint8_t key[32], const uint8_t nonce[12], const void *aad, size_t aadLen, const void *in, size_t len, uint8_t *out)
        {
            chachaXor(key, 1, nonce, (const uint8_t *)in, len, out);
            aeadTag(out + len, key, nonce, (const uint8_t *)aad, aadLen, out, len);
        }

        bool open12(const uint8_t key[32], const uint8_t nonce[12], const void *aad, size_t aadLen, const void *in, size_t len, uint8_t *out)
        {
            if (len < TAG)
                return false;
            const uint8_t *ct = (const uint8_t *)in;
            uint8_t tag[16];
            aeadTag(tag, key, nonce, (const uint8_t *)aad, aadLen, ct, len - TAG);
            if (!equal(tag, ct + len - TAG, TAG))
                return false;
            chachaXor(key, 1, nonce, ct, len - TAG, out);
            return true;
        }

        void seal(const uint8_t key[32], uint64_t nonce, const void *aad, size_t aadLen, const void *in, size_t len, uint8_t *out)
        {
            uint8_t n[12];
            nonceOf(n, nonce);
            seal12(key, n, aad, aadLen, in, len, out);
        }

        bool open(const uint8_t key[32], uint64_t nonce, const void *aad, size_t aadLen, const void *in, size_t len, uint8_t *out)
        {
            uint8_t n[12];
            nonceOf(n, nonce);
            return open12(key, n, aad, aadLen, in, len, out);
        }

        // ------------------------------------------------------------------
        //  SHA-256, HMAC, HKDF
        // ------------------------------------------------------------------
        void sha256(const void *data, size_t len, uint8_t out[32])
        {
            Sha256 s;
            s.update(data, len);
            s.digest(out);
        }

        void hmacSha256(const void *key, size_t keyLen, const void *data, size_t len, uint8_t out[32])
        {
            uint8_t k[64] = {0};
            if (keyLen > 64)
                sha256(key, keyLen, k);
            else
                std::memcpy(k, key, keyLen);
            uint8_t ipad[64], opad[64];
            for (int i = 0; i < 64; ++i)
            {
                ipad[i] = k[i] ^ 0x36;
                opad[i] = k[i] ^ 0x5c;
            }
            uint8_t inner[32];
            Sha256 a;
            a.update(ipad, 64);
            a.update(data, len);
            a.digest(inner);
            Sha256 b;
            b.update(opad, 64);
            b.update(inner, 32);
            b.digest(out);
        }

        void hkdf(const void *salt, size_t saltLen, const void *ikm, size_t ikmLen, const void *info, size_t infoLen, uint8_t *out, size_t outLen)
        {
            uint8_t prk[32];
            static const uint8_t zeros[32] = {0};
            if (saltLen == 0)
                hmacSha256(zeros, 32, ikm, ikmLen, prk);
            else
                hmacSha256(salt, saltLen, ikm, ikmLen, prk);
            uint8_t t[32];
            size_t tLen = 0, done = 0;
            for (uint8_t i = 1; done < outLen; ++i)
            {
                std::string msg((const char *)t, tLen);
                msg.append((const char *)info, infoLen);
                msg += (char)i;
                hmacSha256(prk, 32, msg.data(), msg.size(), t);
                tLen = 32;
                size_t n = outLen - done < 32 ? outLen - done : 32;
                std::memcpy(out + done, t, n);
                done += n;
            }
        }

        bool equal(const void *a, const void *b, size_t len)
        {
            const uint8_t *x = (const uint8_t *)a, *y = (const uint8_t *)b;
            uint8_t acc = 0;
            for (size_t i = 0; i < len; ++i)
                acc |= x[i] ^ y[i];
            return acc == 0;
        }

        std::string toHex(const void *data, size_t len)
        {
            static const char *digits = "0123456789abcdef";
            const uint8_t *p = (const uint8_t *)data;
            std::string out;
            for (size_t i = 0; i < len; ++i)
            {
                out += digits[p[i] >> 4];
                out += digits[p[i] & 15];
            }
            return out;
        }

        bool fromHex(const std::string &hex, void *out, size_t len)
        {
            if (hex.size() != len * 2)
                return false;
            uint8_t *p = (uint8_t *)out;
            for (size_t i = 0; i < len; ++i)
            {
                int v = 0;
                for (int k = 0; k < 2; ++k)
                {
                    char c = hex[2 * i + k];
                    int d = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
                    if (d < 0)
                        return false;
                    v = v * 16 + d;
                }
                p[i] = (uint8_t)v;
            }
            return true;
        }
    }
}
