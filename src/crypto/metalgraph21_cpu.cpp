// Copyright (c) 2025 The Choral Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

// MetalGraph21 Stage 3 — CPU reference graph transform.
// Isolated in its own translation unit so it can be compiled with aggressive
// optimisation flags without affecting the rest of the codebase.

#include <crypto/metalgraph21.h>
#include <crypto/sha256.h>

#include <cstdint>
#include <cstring>

static inline uint32_t rotl32(uint32_t v, unsigned n)
{
    return (v << n) | (v >> (32u - n));
}

void MetalGraph21::TransformGraphCPU(Graph& graph)
{
    // For each node i:
    //   input  = i_LE4 (4 bytes) || edge[0..7] each as LE4 (32 bytes) = 36 bytes total
    //   value  = SHA256(input)
    //   edge[i*8+0] = rotl32(first_word_of_value, 13) XOR original_edge[i*8+0]
    //
    // The transform touches only edge slot 0 of each node; edge slots 1-7 are
    // preserved as routing data.  This matches the Metal shader exactly.

    const uint32_t nc = graph.node_count;
    unsigned char input_buf[4 + EDGES_PER_NODE * 4]; // 36 bytes
    unsigned char hash_out[32];

    for (uint32_t node = 0; node < nc; ++node) {
        // 4-byte little-endian node index
        input_buf[0] = static_cast<unsigned char>(node & 0xff);
        input_buf[1] = static_cast<unsigned char>((node >> 8)  & 0xff);
        input_buf[2] = static_cast<unsigned char>((node >> 16) & 0xff);
        input_buf[3] = static_cast<unsigned char>((node >> 24) & 0xff);

        // 8 × 4-byte little-endian edge values
        const uint32_t base = node * EDGES_PER_NODE;
        for (uint32_t e = 0; e < EDGES_PER_NODE; ++e) {
            const uint32_t v = graph.edges[base + e];
            input_buf[4 + e * 4 + 0] = static_cast<unsigned char>(v & 0xff);
            input_buf[4 + e * 4 + 1] = static_cast<unsigned char>((v >> 8)  & 0xff);
            input_buf[4 + e * 4 + 2] = static_cast<unsigned char>((v >> 16) & 0xff);
            input_buf[4 + e * 4 + 3] = static_cast<unsigned char>((v >> 24) & 0xff);
        }

        CSHA256().Write(input_buf, sizeof(input_buf)).Finalize(hash_out);

        // Extract first 32-bit word (little-endian)
        const uint32_t word = static_cast<uint32_t>(hash_out[0])
                            | (static_cast<uint32_t>(hash_out[1]) << 8)
                            | (static_cast<uint32_t>(hash_out[2]) << 16)
                            | (static_cast<uint32_t>(hash_out[3]) << 24);

        // Mutate edge slot 0
        graph.edges[base] = rotl32(word, 13) ^ graph.edges[base];
    }
}
