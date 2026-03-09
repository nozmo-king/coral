// Copyright (c) 2025 The Choral Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef CORAL_CRYPTO_METALGRAPH21_H
#define CORAL_CRYPTO_METALGRAPH21_H

#include <uint256.h>
#include <cstdint>
#include <vector>

// Forward declaration avoids pulling in block.h from the crypto layer
class CBlockHeader;

namespace MetalGraph21 {

//
// Algorithm parameter set (v0)
// node_count must be a power of two (mask-based edge clamping)
//
static constexpr uint32_t NODE_COUNT_V0  = (1u << 22); // 4,194,304 nodes (~134 MB)
static constexpr uint32_t EDGES_PER_NODE = 8;
static constexpr uint32_t SAMPLE_STRIDE  = 16;          // sample every 16th node in fold

// Domain separation tags
static constexpr char SEED_TAG[] = "CHORAL/METAGRAPH21/SEED";
static constexpr char FOLD_TAG[] = "CHORAL/METAGRAPH21";

// 21e8 motif bytes — every valid block hash contains this pair
static constexpr uint8_t MOTIF_BYTE0 = 0x21;
static constexpr uint8_t MOTIF_BYTE1 = 0xe8;

/**
 * Graph state: flat array of EDGES_PER_NODE uint32 values per node.
 * edges[node * EDGES_PER_NODE + edge_idx] = target node index.
 */
struct Graph {
    uint32_t node_count{NODE_COUNT_V0};
    std::vector<uint32_t> edges;

    void Resize(uint32_t nc)
    {
        node_count = nc;
        edges.assign(static_cast<size_t>(nc) * EDGES_PER_NODE, 0u);
    }
};

/**
 * Stage 1: Derive graph seed from nonce-independent header fields.
 * Computed once per block template; allows reusing the same graph
 * across all nNonce iterations.
 *
 * seed = SHA256d("CHORAL/METAGRAPH21/SEED" || prev_hash || height_LE8 || nTime_LE4 || forest_root)
 */
uint256 DeriveSeed(const uint256& prev_hash,
                   uint64_t       height,
                   uint32_t       nTime,
                   const uint256& forest_root);

uint256 DeriveSeed(const CBlockHeader& header);

/**
 * Stage 2: Populate graph.edges deterministically from seed using ChaCha20.
 * Edges are masked to [0, node_count) via bitmask (requires node_count == 2^k).
 */
void BuildGraph(const uint256& seed, Graph& graph);

/**
 * Stage 3 (CPU path): Transform graph in-place.
 * For each node i:
 *   value    = SHA256(i_LE4 || edges[i*8..i*8+7] each as LE4)
 *   edges[i*8] = rotl32(value[0..3], 13) XOR edges[i*8]
 * Implemented in metalgraph21_cpu.cpp.
 */
void TransformGraphCPU(Graph& graph);

#if defined(__APPLE__)
/**
 * Stage 3 (Metal path): GPU-accelerated transform.
 * Must produce byte-identical output to TransformGraphCPU.
 * Implemented in metalgraph21_metal.mm.
 * Returns false if Metal dispatch fails; caller should fall back to CPU.
 */
bool TransformGraphMetal(Graph& graph);
#endif

/**
 * Stage 4: Fold transformed graph into a SHA3-256 digest.
 *
 * hash = SHA3-256("CHORAL/METAGRAPH21"
 *                  || seed          (32 bytes)
 *                  || nNonce        (8 bytes LE)
 *                  || nExtraNonce   (8 bytes LE)
 *                  || serialized_header
 *                  || sampled_graph_chunks  [every SAMPLE_STRIDE-th node])
 */
void FoldGraph(const Graph&        graph,
               const uint256&      seed,
               uint64_t            nNonce,
               uint64_t            nExtraNonce,
               const CBlockHeader& header,
               uint256&            digest_out);

/**
 * Check the 21e8 motif: scan bytes [0..30] of the 32-byte digest for
 * the two-byte sequence {0x21, 0xe8}.
 * P(success per attempt) ≈ 31/65536 ≈ 1/2115.
 */
bool CheckMotif(const uint256& digest);

/**
 * Top-level: compute the full MetalGraph21 hash for a block header.
 * Runs stages 1–4. Uses Metal GPU on Apple Silicon if available,
 * CPU reference path otherwise.
 */
uint256 Hash(const CBlockHeader& header);

} // namespace MetalGraph21

#endif // CORAL_CRYPTO_METALGRAPH21_H
