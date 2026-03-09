// Copyright (c) 2025 The Choral Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include <arith_uint256.h>
#include <crypto/metalgraph21.h>
#include <pow.h>
#include <primitives/block.h>
#include <test/util/setup_common.h>
#include <uint256.h>

BOOST_FIXTURE_TEST_SUITE(metalgraph21_tests, BasicTestingSetup)

// ---------------------------------------------------------------------------
// Test 1: Seed derivation is a pure function of its inputs
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(seed_determinism)
{
    uint256 prev, forest;
    prev.SetNull();
    forest.SetNull();

    const uint256 s1 = MetalGraph21::DeriveSeed(prev, 0, 1234567, forest);
    const uint256 s2 = MetalGraph21::DeriveSeed(prev, 0, 1234567, forest);
    BOOST_CHECK_EQUAL(s1, s2);

    // Different height → different seed
    const uint256 s3 = MetalGraph21::DeriveSeed(prev, 1, 1234567, forest);
    BOOST_CHECK(s1 != s3);

    // Different time → different seed
    const uint256 s4 = MetalGraph21::DeriveSeed(prev, 0, 1234568, forest);
    BOOST_CHECK(s1 != s4);
}

// ---------------------------------------------------------------------------
// Test 2: Graph construction is deterministic and all edges are in range
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(graph_build_determinism)
{
    uint256 seed;
    seed.SetNull();

    MetalGraph21::Graph g1, g2;
    g1.Resize(MetalGraph21::NODE_COUNT_V0);
    g2.Resize(MetalGraph21::NODE_COUNT_V0);

    MetalGraph21::BuildGraph(seed, g1);
    MetalGraph21::BuildGraph(seed, g2);

    BOOST_CHECK(g1.edges == g2.edges);

    // All edge indices must be within [0, node_count)
    for (const auto e : g1.edges) {
        BOOST_CHECK(e < MetalGraph21::NODE_COUNT_V0);
    }
}

// ---------------------------------------------------------------------------
// Test 3: CPU transform is deterministic
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(transform_cpu_determinism)
{
    uint256 seed;
    seed.SetNull();

    MetalGraph21::Graph g1, g2;
    g1.Resize(MetalGraph21::NODE_COUNT_V0);
    g2.Resize(MetalGraph21::NODE_COUNT_V0);

    MetalGraph21::BuildGraph(seed, g1);
    MetalGraph21::BuildGraph(seed, g2);

    MetalGraph21::TransformGraphCPU(g1);
    MetalGraph21::TransformGraphCPU(g2);

    BOOST_CHECK(g1.edges == g2.edges);
}

// ---------------------------------------------------------------------------
// Test 4: 21e8 motif detection — correct boundary behaviour
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(motif_check_boundaries)
{
    uint256 digest;

    // All-zero digest has no motif
    digest.SetNull();
    BOOST_CHECK(!MetalGraph21::CheckMotif(digest));

    // Inject at byte 0
    digest.SetNull();
    digest.begin()[0] = 0x21;
    digest.begin()[1] = 0xe8;
    BOOST_CHECK(MetalGraph21::CheckMotif(digest));

    // Inject at byte 15 (middle)
    digest.SetNull();
    digest.begin()[15] = 0x21;
    digest.begin()[16] = 0xe8;
    BOOST_CHECK(MetalGraph21::CheckMotif(digest));

    // Inject at byte 30 (last valid window: indices 30 and 31)
    digest.SetNull();
    digest.begin()[30] = 0x21;
    digest.begin()[31] = 0xe8;
    BOOST_CHECK(MetalGraph21::CheckMotif(digest));

    // Only first byte present, no match
    digest.SetNull();
    digest.begin()[5] = 0x21;
    BOOST_CHECK(!MetalGraph21::CheckMotif(digest));

    // Bytes reversed — no match
    digest.SetNull();
    digest.begin()[5] = 0xe8;
    digest.begin()[6] = 0x21;
    BOOST_CHECK(!MetalGraph21::CheckMotif(digest));
}

// ---------------------------------------------------------------------------
// Test 5: FoldGraph is nonce-sensitive
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(fold_nonce_sensitivity)
{
    uint256 seed;
    seed.SetNull();

    MetalGraph21::Graph g;
    g.Resize(MetalGraph21::NODE_COUNT_V0);
    MetalGraph21::BuildGraph(seed, g);
    MetalGraph21::TransformGraphCPU(g);

    CBlockHeader hdr;
    hdr.nVersion = 1;
    hdr.hashPrevBlock.SetNull();
    hdr.nHeight = 0;
    hdr.nTime = 1700000000;
    hdr.forest_root.SetNull();
    hdr.nBits = 0x207fffff;
    hdr.nBits_receipt = hdr.nBits;
    hdr.nBits_service = hdr.nBits;
    hdr.nExtraNonce = 0;

    uint256 d1, d2;
    hdr.nNonce = 42;
    MetalGraph21::FoldGraph(g, seed, hdr.nNonce, hdr.nExtraNonce, hdr, d1);
    hdr.nNonce = 43;
    MetalGraph21::FoldGraph(g, seed, hdr.nNonce, hdr.nExtraNonce, hdr, d2);

    BOOST_CHECK(d1 != d2);
}

// ---------------------------------------------------------------------------
// Test 6: End-to-end round-trip on a very easy target
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(pow_roundtrip_easy)
{
    const auto& chainParams = m_node.chainman->GetParams();
    const Consensus::Params& consensus = chainParams.GetConsensus();

    CBlockHeader hdr;
    hdr.nVersion      = 1;
    hdr.hashPrevBlock.SetNull();
    hdr.nHeight       = 0;
    hdr.nTime         = 1700000000;
    hdr.forest_root.SetNull();
    hdr.nBits         = 0x207fffff; // very easy regtest target
    hdr.nBits_receipt = hdr.nBits;
    hdr.nBits_service = hdr.nBits;
    hdr.nExtraNonce   = 0;

    const arith_uint256 target = arith_uint256().SetCompact(hdr.nBits);

    bool found = false;
    for (uint64_t nonce = 0; nonce < 10'000'000ULL; ++nonce) {
        hdr.nNonce = nonce;
        const uint256 digest = MetalGraph21::Hash(hdr);
        if (MetalGraph21::CheckMotif(digest) &&
            UintToArith256(digest) <= target) {
            // Must also pass the full PoW checker
            BOOST_CHECK(CheckMetalGraph21PoW(hdr, hdr.nBits, consensus));
            found = true;
            break;
        }
    }

    BOOST_CHECK_MESSAGE(found,
        "MetalGraph21 round-trip: no valid hash found in 10M iterations on easy target");
}

BOOST_AUTO_TEST_SUITE_END()
