// Copyright (c) 2025 The Choral Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

// MetalGraph21 Stage 3 — Metal GPU dispatch (Apple Silicon / macOS only).
// Compiled only when HAVE_METAL is set by the build system.

#include <crypto/metalgraph21.h>
#include <logging.h>

#if defined(__APPLE__)

#import <Metal/Metal.h>
#import <Foundation/Foundation.h>
#include <cstdint>

// ---------------------------------------------------------------------------
// Metal kernel source (compiled at runtime for v0 simplicity).
// Must produce byte-identical output to TransformGraphCPU.
// Full SHA-256 is implemented inline in Metal Shading Language since
// metal_stdlib does not provide a SHA-256 primitive.
// ---------------------------------------------------------------------------
static NSString* const kMG21ShaderSource = @
"#include <metal_stdlib>\n"
"using namespace metal;\n"
"\n"
"// SHA-256 constants\n"
"constant uint K[64] = {\n"
"  0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,\n"
"  0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,\n"
"  0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,\n"
"  0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,\n"
"  0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,\n"
"  0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,\n"
"  0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,\n"
"  0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,\n"
"  0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,\n"
"  0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,\n"
"  0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,\n"
"  0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,\n"
"  0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,\n"
"  0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,\n"
"  0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,\n"
"  0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u\n"
"};\n"
"\n"
"// Compute SHA-256 of a 36-byte input block (fixed length, no padding loop needed)\n"
"uint sha256_first_word(thread uchar* msg, uint msg_len) {\n"
"  // Pad to 64 bytes: msg || 0x80 || zeros || length_BE64\n"
"  uchar block[64];\n"
"  for (uint i = 0; i < msg_len; ++i) block[i] = msg[i];\n"
"  block[msg_len] = 0x80u;\n"
"  for (uint i = msg_len + 1; i < 56; ++i) block[i] = 0;\n"
"  // Length in bits as big-endian 64-bit: msg_len * 8\n"
"  ulong bit_len = (ulong)msg_len * 8;\n"
"  for (int i = 0; i < 8; ++i)\n"
"    block[56 + i] = (uchar)((bit_len >> (56 - 8*i)) & 0xff);\n"
"\n"
"  // Schedule\n"
"  uint w[64];\n"
"  for (uint i = 0; i < 16; ++i) {\n"
"    w[i] = ((uint)block[i*4]   << 24) | ((uint)block[i*4+1] << 16)\n"
"          |((uint)block[i*4+2] <<  8) |  (uint)block[i*4+3];\n"
"  }\n"
"  for (uint i = 16; i < 64; ++i) {\n"
"    uint s0 = rotate(w[i-15], 32u-7u)  ^ rotate(w[i-15], 32u-18u) ^ (w[i-15] >> 3);\n"
"    uint s1 = rotate(w[i-2],  32u-17u) ^ rotate(w[i-2],  32u-19u) ^ (w[i-2]  >> 10);\n"
"    w[i] = w[i-16] + s0 + w[i-7] + s1;\n"
"  }\n"
"\n"
"  // Compress\n"
"  uint h0=0x6a09e667u, h1=0xbb67ae85u, h2=0x3c6ef372u, h3=0xa54ff53au;\n"
"  uint h4=0x510e527fu, h5=0x9b05688cu, h6=0x1f83d9abu, h7=0x5be0cd19u;\n"
"  uint a=h0,b=h1,c=h2,d=h3,e=h4,f=h5,g=h6,h=h7;\n"
"  for (uint i = 0; i < 64; ++i) {\n"
"    uint S1   = rotate(e,32u-6u) ^ rotate(e,32u-11u) ^ rotate(e,32u-25u);\n"
"    uint ch   = (e & f) ^ (~e & g);\n"
"    uint temp1 = h + S1 + ch + K[i] + w[i];\n"
"    uint S0   = rotate(a,32u-2u) ^ rotate(a,32u-13u) ^ rotate(a,32u-22u);\n"
"    uint maj  = (a & b) ^ (a & c) ^ (b & c);\n"
"    uint temp2 = S0 + maj;\n"
"    h=g; g=f; f=e; e=d+temp1;\n"
"    d=c; c=b; b=a; a=temp1+temp2;\n"
"  }\n"
"  return h0 + a; // return first word of digest\n"
"}\n"
"\n"
"kernel void mg21_transform(\n"
"    device uint* edges         [[ buffer(0) ]],\n"
"    constant uint& node_count  [[ buffer(1) ]],\n"
"    uint gid                   [[ thread_position_in_grid ]])\n"
"{\n"
"  if (gid >= node_count) return;\n"
"  const uint EDGES = 8;\n"
"  const uint base = gid * EDGES;\n"
"\n"
"  // Build 36-byte input: 4-byte node index LE || 8 * 4-byte edge values LE\n"
"  uchar input[36];\n"
"  input[0] = (uchar)(gid & 0xff);\n"
"  input[1] = (uchar)((gid >> 8)  & 0xff);\n"
"  input[2] = (uchar)((gid >> 16) & 0xff);\n"
"  input[3] = (uchar)((gid >> 24) & 0xff);\n"
"  for (uint e = 0; e < EDGES; ++e) {\n"
"    uint v = edges[base + e];\n"
"    input[4 + e*4 + 0] = (uchar)(v & 0xff);\n"
"    input[4 + e*4 + 1] = (uchar)((v >> 8)  & 0xff);\n"
"    input[4 + e*4 + 2] = (uchar)((v >> 16) & 0xff);\n"
"    input[4 + e*4 + 3] = (uchar)((v >> 24) & 0xff);\n"
"  }\n"
"\n"
"  uint word = sha256_first_word(input, 36);\n"
"\n"
"  // rotl32(word, 13) XOR original edge[0]\n"
"  uint orig = edges[base];\n"
"  uint rotated = (word << 13u) | (word >> 19u);\n"
"  edges[base] = rotated ^ orig;\n"
"}\n";

// ---------------------------------------------------------------------------
// TransformGraphMetal — dispatches the Metal kernel
// ---------------------------------------------------------------------------

bool MetalGraph21::TransformGraphMetal(Graph& graph)
{
    @autoreleasepool {
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        if (!device) {
            LogPrint(BCLog::NET, "MetalGraph21: No Metal device available\n");
            return false;
        }

        NSError* error = nil;

        // Compile kernel from source (v0; swap to precompiled .metallib later)
        MTLCompileOptions* opts = [[MTLCompileOptions alloc] init];
        id<MTLLibrary> lib = [device newLibraryWithSource:kMG21ShaderSource
                                                  options:opts
                                                    error:&error];
        if (!lib) {
            LogPrintf("MetalGraph21: Shader compile error: %s\n",
                      [[error localizedDescription] UTF8String]);
            return false;
        }

        id<MTLFunction> fn = [lib newFunctionWithName:@"mg21_transform"];
        if (!fn) {
            LogPrintf("MetalGraph21: Function 'mg21_transform' not found\n");
            return false;
        }

        id<MTLComputePipelineState> pso =
            [device newComputePipelineStateWithFunction:fn error:&error];
        if (!pso) {
            LogPrintf("MetalGraph21: PSO creation failed: %s\n",
                      [[error localizedDescription] UTF8String]);
            return false;
        }

        // Allocate MTLBuffer (copy path for v0 — avoids alignment requirements)
        const size_t edge_bytes =
            static_cast<size_t>(graph.node_count) * EDGES_PER_NODE * sizeof(uint32_t);

        id<MTLBuffer> buf = [device newBufferWithBytes:graph.edges.data()
                                                length:edge_bytes
                                               options:MTLResourceStorageModeShared];
        if (!buf) {
            LogPrintf("MetalGraph21: Failed to allocate MTLBuffer\n");
            return false;
        }

        uint32_t nc = graph.node_count;

        // Encode dispatch
        id<MTLCommandQueue>         queue  = [device newCommandQueue];
        id<MTLCommandBuffer>        cmdbuf = [queue commandBuffer];
        id<MTLComputeCommandEncoder> enc   = [cmdbuf computeCommandEncoder];

        [enc setComputePipelineState:pso];
        [enc setBuffer:buf offset:0 atIndex:0];
        [enc setBytes:&nc length:sizeof(nc) atIndex:1];

        NSUInteger tg = [pso maxTotalThreadsPerThreadgroup];
        MTLSize threadsPerGrid   = { graph.node_count, 1, 1 };
        MTLSize threadsPerGroup  = { tg, 1, 1 };
        [enc dispatchThreads:threadsPerGrid threadsPerThreadgroup:threadsPerGroup];

        [enc endEncoding];
        [cmdbuf commit];
        [cmdbuf waitUntilCompleted];

        if ([cmdbuf status] != MTLCommandBufferStatusCompleted) {
            LogPrintf("MetalGraph21: Command buffer did not complete\n");
            return false;
        }

        // Copy results back into graph
        memcpy(graph.edges.data(), [buf contents], edge_bytes);

        LogPrint(BCLog::NET, "MetalGraph21: GPU transform complete (%u nodes)\n",
                 graph.node_count);
        return true;
    }
}

#endif // __APPLE__
