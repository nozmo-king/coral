// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Coral Core developers
// Copyright (c) 2024 The Coral Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <pow.h>

#include <arith_uint256.h>
#include <chain.h>
#include <primitives/block.h>
#include <uint256.h>
#include <logging.h>
#include <util/strencodings.h>
#include <streams.h>
#include <version.h>

#include <crypto/metalgraph21.h>

unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{
    assert(pindexLast != nullptr);
    
    // Coral: Special case for block #1 - 1000x higher difficulty
    if ((pindexLast->nHeight + 1) == 1) {
        // Make block #1 1000x harder than genesis
        arith_uint256 bnLimit = UintToArith256(params.powLimit);
        arith_uint256 bnTarget = bnLimit / 1000;
        return bnTarget.GetCompact();
    }
    unsigned int nProofOfWorkLimit = UintToArith256(params.powLimit).GetCompact();

    // Only change once per difficulty adjustment interval
    if ((pindexLast->nHeight+1) % params.DifficultyAdjustmentInterval() != 0)
    {
        if (params.fPowAllowMinDifficultyBlocks)
        {
            // Special difficulty rule for testnet:
            // If the new block's timestamp is more than 2* 10 minutes
            // then allow mining of a min-difficulty block.
            if (pblock->GetBlockTime() > pindexLast->GetBlockTime() + params.nPowTargetSpacing*2)
                return nProofOfWorkLimit;
            else
            {
                // Return the last non-special-min-difficulty-rules-block
                const CBlockIndex* pindex = pindexLast;
                while (pindex->pprev && pindex->nHeight % params.DifficultyAdjustmentInterval() != 0 && pindex->nBits == nProofOfWorkLimit)
                    pindex = pindex->pprev;
                return pindex->nBits;
            }
        }
        return pindexLast->nBits;
    }

    // Go back by what we want to be 14 days worth of blocks
    int nHeightFirst = pindexLast->nHeight - (params.DifficultyAdjustmentInterval()-1);
    assert(nHeightFirst >= 0);
    const CBlockIndex* pindexFirst = pindexLast->GetAncestor(nHeightFirst);
    assert(pindexFirst);

    return CalculateNextWorkRequired(pindexLast, pindexFirst->GetBlockTime(), params);
}

unsigned int CalculateNextWorkRequired(const CBlockIndex* pindexLast, int64_t nFirstBlockTime, const Consensus::Params& params)
{
    if (params.fPowNoRetargeting)
        return pindexLast->nBits;

    // Limit adjustment step
    int64_t nActualTimespan = pindexLast->GetBlockTime() - nFirstBlockTime;
    if (nActualTimespan < params.nPowTargetTimespan/4)
        nActualTimespan = params.nPowTargetTimespan/4;
    if (nActualTimespan > params.nPowTargetTimespan*4)
        nActualTimespan = params.nPowTargetTimespan*4;

    // Retarget
    const arith_uint256 bnPowLimit = UintToArith256(params.powLimit);
    arith_uint256 bnNew;
    bnNew.SetCompact(pindexLast->nBits);
    bnNew *= nActualTimespan;
    bnNew /= params.nPowTargetTimespan;

    if (bnNew > bnPowLimit)
        bnNew = bnPowLimit;

    return bnNew.GetCompact();
}

// Check that on difficulty adjustments, the new difficulty does not increase
// or decrease beyond the permitted limits.
bool PermittedDifficultyTransition(const Consensus::Params& params, int64_t height, uint32_t old_nbits, uint32_t new_nbits)
{
    if (params.fPowAllowMinDifficultyBlocks) return true;

    if (height % params.DifficultyAdjustmentInterval() == 0) {
        int64_t smallest_timespan = params.nPowTargetTimespan/4;
        int64_t largest_timespan = params.nPowTargetTimespan*4;

        const arith_uint256 pow_limit = UintToArith256(params.powLimit);
        arith_uint256 observed_new_target;
        observed_new_target.SetCompact(new_nbits);

        // Calculate the largest difficulty value possible:
        arith_uint256 largest_difficulty_target;
        largest_difficulty_target.SetCompact(old_nbits);
        largest_difficulty_target *= largest_timespan;
        largest_difficulty_target /= params.nPowTargetTimespan;

        if (largest_difficulty_target > pow_limit) {
            largest_difficulty_target = pow_limit;
        }

        // Round and then compare this new calculated value to what is
        // observed.
        arith_uint256 maximum_new_target;
        maximum_new_target.SetCompact(largest_difficulty_target.GetCompact());
        if (maximum_new_target < observed_new_target) return false;

        // Calculate the smallest difficulty value possible:
        arith_uint256 smallest_difficulty_target;
        smallest_difficulty_target.SetCompact(old_nbits);
        smallest_difficulty_target *= smallest_timespan;
        smallest_difficulty_target /= params.nPowTargetTimespan;

        if (smallest_difficulty_target > pow_limit) {
            smallest_difficulty_target = pow_limit;
        }

        // Round and then compare this new calculated value to what is
        // observed.
        arith_uint256 minimum_new_target;
        minimum_new_target.SetCompact(smallest_difficulty_target.GetCompact());
        if (minimum_new_target > observed_new_target) return false;
    } else if (old_nbits != new_nbits) {
        return false;
    }
    return true;
}

bool CheckProofOfWork(uint256 hash, unsigned int nBits, const Consensus::Params& params)
{
    bool fNegative;
    bool fOverflow;
    arith_uint256 bnTarget;

    bnTarget.SetCompact(nBits, &fNegative, &fOverflow);

    // Check range
    if (fNegative || bnTarget == 0 || fOverflow || bnTarget > UintToArith256(params.powLimit))
        return false;

    // Check proof of work matches claimed amount
    if (UintToArith256(hash) > bnTarget)
        return false;

    return true;
}

// ---------------------------------------------------------------------------
// MetalGraph21 graph cache — avoids rebuilding the 134 MB graph per nonce.
// Keyed on the nonce-independent seed. Single-threaded miner; make
// thread_local if multi-threaded mining is added in future.
// ---------------------------------------------------------------------------
namespace {
struct MG21Cache {
    uint256             cached_seed;
    MetalGraph21::Graph graph;
    bool                valid{false};
};
static MG21Cache g_mg21_cache;
} // namespace

uint256 GetMetalGraph21Hash(const CBlockHeader& block)
{
    return MetalGraph21::Hash(block);
}

bool CheckMetalGraph21PoW(const CBlockHeader& block,
                           unsigned int nBits,
                           const Consensus::Params& params)
{
    const uint256 digest = MetalGraph21::Hash(block);
    if (!MetalGraph21::CheckMotif(digest)) {
        return false;
    }
    return CheckProofOfWork(digest, nBits, params);
}

bool ScanMetalGraph21Hash(CBlockHeader* pblock,
                           uint64_t& nNonce,
                           uint32_t nHashesDone,
                           const Consensus::Params& consensusParams)
{
    const arith_uint256 target = arith_uint256().SetCompact(pblock->nBits);

    const uint256 seed = MetalGraph21::DeriveSeed(*pblock);

    if (!g_mg21_cache.valid || g_mg21_cache.cached_seed != seed) {
        g_mg21_cache.graph.Resize(MetalGraph21::NODE_COUNT_V0);
        MetalGraph21::BuildGraph(seed, g_mg21_cache.graph);

        bool metal_ok = false;
#if defined(__APPLE__)
        metal_ok = MetalGraph21::TransformGraphMetal(g_mg21_cache.graph);
#endif
        if (!metal_ok) {
            MetalGraph21::TransformGraphCPU(g_mg21_cache.graph);
        }

        g_mg21_cache.cached_seed = seed;
        g_mg21_cache.valid       = true;
        LogPrintf("MetalGraph21: Built graph for seed %s\n", seed.ToString());
    }

    uint256 digest;
    for (uint32_t i = 0; i < nHashesDone; ++i) {
        pblock->nNonce = nNonce;

        MetalGraph21::FoldGraph(g_mg21_cache.graph, seed,
                                nNonce, pblock->nExtraNonce,
                                *pblock, digest);

        if (MetalGraph21::CheckMotif(digest) &&
            UintToArith256(digest) <= target) {
            return true;
        }

        ++nNonce;

        if ((i & 0x3ffu) == 0 && ShutdownRequested()) {
            return false;
        }
    }
    return false;
}

