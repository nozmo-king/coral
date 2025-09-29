#!/bin/bash

echo "🪸 Coral Cryptocurrency CLI Demo 🪸"
echo "======================================"
echo ""

echo "1. START DAEMON:"
echo "   corald -daemon"
echo ""

echo "2. GENERATE NEW ADDRESS:"
echo "   coral-cli getnewaddress"
echo "   Output: 1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa  (example)"
echo ""

echo "3. GET WALLET INFO:"
echo "   coral-cli getwalletinfo"
echo "   Shows: balance, transaction count, keypool size"
echo ""

echo "4. CHECK BLOCKCHAIN STATUS:"
echo "   coral-cli getblockchaininfo"
echo "   Shows: chain, blocks, difficulty, verification progress"
echo ""

echo "5. MINING COMMANDS:"
echo "   # Mine 1 block to specific address"
echo "   coral-cli generatetoaddress 1 \"1YourCoralAddress\""
echo ""
echo "   # Mine 5 blocks to wallet"
echo "   coral-cli generatetodescriptor 5 \"wpkh([d34db33f/84h/0h/0h]xpub...)\""
echo ""

echo "6. CHECK MINING STATUS:"
echo "   coral-cli getmininginfo"
echo "   Shows: blocks, difficulty, networkhashps, pooledtx"
echo ""

echo "7. TRANSACTION COMMANDS:"
echo "   coral-cli listreceivedbyaddress  # Show received payments"
echo "   coral-cli listtransactions       # Show all transactions"
echo "   coral-cli getbalance            # Show wallet balance"
echo ""

echo "8. NETWORK COMMANDS:"
echo "   coral-cli getnetworkinfo        # Network information"
echo "   coral-cli getpeerinfo          # Connected peers"
echo "   coral-cli getconnectioncount   # Number of connections"
echo ""

echo "9. STOP DAEMON:"
echo "   coral-cli stop"
echo ""

echo "🔥 MINING PERFORMANCE EXPECTATIONS:"
echo "   - CPU Mining: 500-8000 H/s (depends on processor)"
echo "   - Memory Usage: ~2GB per mining thread"
echo "   - Block Time: ~10 minutes target"
echo "   - Algorithm: RandomX (ASIC-resistant)"
echo ""

echo "📁 CONFIGURATION FILE (~/.coral/coral.conf):"
echo "   rpcuser=coraluser"
echo "   rpcpassword=your_secure_password"
echo "   server=1"
echo "   daemon=1"
echo "   listen=1"
echo "   txindex=1"
echo ""

echo "✅ All commands use standard Bitcoin RPC format!"
echo "🪸 Ready for CPU democratic mining!"