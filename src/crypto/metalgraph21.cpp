// Copyright (c) 2025 The Choral Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

// MetalGraph21 PoW — Stages 1 (seed), 2 (graph build), 4 (fold), motif check,
// and top-level Hash() dispatcher.

#include <crypto/metalgraph21.h>

#include <crypto/chacha20.h>
#include <crypto/sha256.h>
#include <crypto/sha3.h>
#include <logging.h>
#include <primitives/block.h>
#include <serialize.h>
#include <span.h>
#include <streams.h>
#include <version.h>

#include <cstring>

// ---------------------------------------------------------------------------
// Helper: write a 64-bit value as 8 bytes little-endian into a buffer
// ---------------------------------------------------------------------------
static void WriteLE64(unsigned char* p, uint64_t v)
{
    for (int i = 0; i < 8; ++i) {
        p[i] = static_cast<unsigned char>((v >> (8 * i)) & 0xff);
    }
}

// ---------------------------------------------------------------------------
// Helper: write a 32-bit value as 4 bytes little-endian into a buffer
// ---------------------------------------------------------------------------
static void WriteLE32(unsigned char* p, uint32_t v)
{
    for (int i = 0; i < 4; ++i) {
        p[i] = static_cast<unsigned char>((v >> (8 * i)) & 0xff);
    }
}

// ---------------------------------------------------------------------------
// Stage 1 — graph seed derivation
// ---------------------------------------------------------------------------

uint256 MetalGraph21::DeriveSeed(const uint256& prev_hash,
                                  uint64_t       height,
                                  uint32_t       nTime,
                                  const uint256& forest_root)
{
    // seed = SHA256d( tag || prev_hash || height_LE8 || nTime_LE4 || forest_root )
    constexpr size_t TAG_LEN = sizeof(SEED_TAG) - 1; // exclude null terminator
    unsigned char buf[TAG_LEN + 32 + 8 + 4 + 32];
    size_t off = 0;

    memcpy(buf + off, SEED_TAG, TAG_LEN);
    off += TAG_LEN;

    memcpy(buf + off, prev_hash.begin(), 32);
    off += 32;

    WriteLE64(buf + off, height);
    off += 8;

    WriteLE32(buf + off, nTime);
    off += 4;

    memcpy(buf + off, forest_root.begin(), 32);
    off += 32;

    // Double-SHA256
    uint256 mid, result;
    CSHA256().Write(buf, off).Finalize(mid.begin());
    CSHA256().Write(mid.begin(), 32).Finalize(result.begin());
    return result;
}

uint256 MetalGraph21::DeriveSeed(const CBlockHeader& header)
{
    return DeriveSeed(header.hashPrevBlock,
                      header.nHeight,
                      header.nTime,
                      header.forest_root);
}

// ---------------------------------------------------------------------------
// Stage 2 — graph construction via ChaCha20 PRNG
// ---------------------------------------------------------------------------

void MetalGraph21::BuildGraph(const uint256& seed, Graph& graph)
{
    // ChaCha20 keyed from the 32-byte seed, IV = 0, starting at block 0.
    // Generates EDGES_PER_NODE * 4 bytes of keystream per node sequentially.
    ChaCha20 prng(seed.begin(), 32);
    prng.SetIV(0);
    prng.Seek(0);

    const size_t total_bytes =
        static_cast<size_t>(graph.node_count) * EDGES_PER_NODE * sizeof(uint32_t);

    prng.Keystream(reinterpret_cast<unsigned char*>(graph.edges.data()), total_bytes);

    // Clamp all edge indices to [0, node_count).
    // node_count is a power of two, so a bitmask is exact.
    const uint32_t mask = graph.node_count - 1u;
    for (auto& e : graph.edges) {
        e &= mask;
    }
}

// ---------------------------------------------------------------------------
// Stage 4 — SHA3-256 fold + 21e8 motif check
// ---------------------------------------------------------------------------

void MetalGraph21::FoldGraph(const Graph&        graph,
                              const uint256&      seed,
                              uint64_t            nNonce,
                              uint64_t            nExtraNonce,
                              const CBlockHeader& header,
                              uint256&            digest_out)
{
    SHA3_256 hasher;

    // Domain tag
    hasher.Write(Span<const unsigned char>(
        reinterpret_cast<const unsigned char*>(FOLD_TAG),
        sizeof(FOLD_TAG) - 1));

    // Seed (32 bytes)
    hasher.Write(Span<const unsigned char>(seed.begin(), 32));

    // nNonce (8 bytes LE)
    unsigned char nonce_buf[8];
    WriteLE64(nonce_buf, nNonce);
    hasher.Write(Span<const unsigned char>(nonce_buf, 8));

    // nExtraNonce (8 bytes LE)
    unsigned char extra_buf[8];
    WriteLE64(extra_buf, nExtraNonce);
    hasher.Write(Span<const unsigned char>(extra_buf, 8));

    // Serialised header (canonical wire format, includes current nNonce)
    CDataStream ss(SER_NETWORK, INIT_PROTO_VERSION);
    ss << header;
    hasher.Write(Span<const unsigned char>(
        reinterpret_cast<const unsigned char*>(ss.data()), ss.size()));

    // Sampled graph chunks: every SAMPLE_STRIDE-th node contributes 32 bytes
    const unsigned char* edge_bytes =
        reinterpret_cast<const unsigned char*>(graph.edges.data());
    const size_t node_stride = EDGES_PER_NODE * sizeof(uint32_t); // 32 bytes

    for (uint32_t n = 0; n < graph.node_count; n += SAMPLE_STRIDE) {
        hasher.Write(Span<const unsigned char>(
            edge_bytes + static_cast<size_t>(n) * node_stride, node_stride));
    }

    hasher.Finalize(Span<unsigned char>(digest_out.begin(), 32));
}

bool MetalGraph21::CheckMotif(const uint256& digest)
{
    const unsigned char* d = digest.begin();
    for (int i = 0; i <= 30; ++i) {
        if (d[i] == MOTIF_BYTE0 && d[i + 1] == MOTIF_BYTE1) {
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// Top-level Hash() — runs all 4 stages
// ---------------------------------------------------------------------------

uint256 MetalGraph21::Hash(const CBlockHeader& header)
{
    // Stage 1
    const uint256 seed = DeriveSeed(header);

    // Stage 2
    Graph graph;
    graph.Resize(NODE_COUNT_V0);
    BuildGraph(seed, graph);

    // Stage 3 — Metal GPU if available, CPU otherwise
    bool transform_done = false;
#if defined(__APPLE__)
    transform_done = TransformGraphMetal(graph);
#endif
    if (!transform_done) {
        TransformGraphCPU(graph);
    }

    // Stage 4
    uint256 digest;
    FoldGraph(graph, seed, header.nNonce, header.nExtraNonce, header, digest);
    return digest;
}
