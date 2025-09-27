# Creating GitHub Release with Windows Installer

## Current Status
The Windows installer `Coral-1.0.0-Windows-Setup.exe` (788KB) has been built successfully but needs to be uploaded to GitHub Releases for public download.

## Files Ready for Release
- `Coral-1.0.0-Windows-Setup.exe` - Complete Windows installer (788KB)
- `coral-test.exe` - Test executable (2.8MB)
- `bin/coral-wallet.bat` - Wallet helper script
- `bin/coral-mine.bat` - Mining helper script
- `build-win64/README-Windows.txt` - Windows documentation

## Manual Release Creation Steps

### 1. Go to GitHub Repository
Visit: https://github.com/nozmo-king/coral

### 2. Create New Release
1. Click "Releases" in the right sidebar
2. Click "Create a new release"
3. Set tag version: `v1.0.0`
4. Set release title: `Coral v1.0.0 - Windows Installer & CPU Mining`

### 3. Release Description
```markdown
# 🪸 Coral Cryptocurrency v1.0.0 - Windows Release

**The RandomX CPU Mining Revolution is here!**

## What's New
✅ Complete Windows installer with GUI
✅ RandomX CPU mining algorithm (ASIC-resistant)
✅ Genesis block with 21e800 difficulty
✅ Polish genesis message embedded
✅ Full Bitcoin → Coral rebrand complete

## Windows Installation
1. Download `Coral-1.0.0-Windows-Setup.exe` below
2. Run as Administrator
3. Launch "Coral Mining" from Start Menu
4. Expected performance: 500-8000 H/s (CPU dependent)

## Network Specifications
- **Algorithm**: RandomX (CPU-optimized)
- **Block Time**: 10 minutes
- **Block Reward**: 100 CORAL
- **Port**: 8334
- **Genesis Difficulty**: 21e800 (impossible to manipulate)

## Files in this Release
- `Coral-1.0.0-Windows-Setup.exe` - Complete Windows installer
- `coral-test.exe` - Test executable
- `README-Windows.txt` - Windows documentation

## System Requirements
- Windows 10/11 (64-bit)
- 8GB+ RAM (recommended)
- Multi-core CPU (more cores = better mining)
- 50GB+ free storage
- Stable internet connection

## Security Warning
This is experimental software. Only invest what you can afford to lose.
Always backup your wallet and never share private keys.

---
🪸 **Welcome to Democratic CPU Mining!** 🪸
```

### 4. Upload Files
Drag and drop these files to the release:
- `Coral-1.0.0-Windows-Setup.exe`
- `coral-test.exe`
- `build-win64/README-Windows.txt`

### 5. Publish Release
- Check "Set as the latest release"
- Click "Publish release"

## Expected Download Links
After release creation, files will be available at:
- https://github.com/nozmo-king/coral/releases/download/v1.0.0/Coral-1.0.0-Windows-Setup.exe
- https://github.com/nozmo-king/coral/releases/download/v1.0.0/coral-test.exe
- https://github.com/nozmo-king/coral/releases/download/v1.0.0/README-Windows.txt

## Update README Links
After release is live, update README.md download links to point to the actual release URLs.

---

**File Locations on Server:**
- Main installer: `/root/coral/Coral-1.0.0-Windows-Setup.exe` (788KB)
- Test exe: `/root/coral/coral-test.exe` (2.8MB)
- Documentation: `/root/coral/build-win64/README-Windows.txt`