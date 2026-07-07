# Contributing to Torvik

Thank you for your interest in contributing to **Torvik**!  
This is a new self-hosting, compiled programming language, and community contributions are essential to its growth.

All contributors are expected to follow our [Code of Conduct](CODE_OF_CONDUCT.md)

## How Can I Contribute?

### 1. Reporting Bugs or Suggesting Features

- Use the [Issue tracker](https://github.com/torvik-lang/torvik/issues).
- For bugs, please include:
  - Torvik version (`rune version`)
  - Minimal code example that reproduces the issue
  - Expected vs. actual behavior
  - Platform / OS details
- For feature requests, describe the problem it solves and why it fits Torvik’s goals.

### 2. Submitting Code Changes (Pull Requests)

We welcome contributions in the following areas:
- Compiler improvements (`src/`)
- Standard library and runtime (`runtime/`) (`src/std/`)
- Documentation and examples (`docs/`)
- Package manager (`rune`) enhancements (`src/rune.tv`)
- Bug fixes and performance improvements
- New language features (after discussing first)

**Before submitting a PR:**
1. Open an issue first (or comment on an existing one) to discuss your proposed change — especially for new features or larger refactors.
2. Fork the repository and create a feature branch (`git checkout -b feature/amazing-change`).
3. Make your changes following the project's style and self-hosting principles.
4. Test your changes thoroughly (manual compilation if possible or use the scripts we'll give you for automation. if not able to test, we can test on our side).
5. Update documentation if needed.
6. Submit the Pull Request.

### Development Setup

```bash
# Clone the repo
git clone https://github.com/torvik-lang/torvik.git
cd torvik

# Build: For building Torvik directly, contact us at torviklang@gmail.com and we will send you the necessary scripts for building it and the torvc binary if needed. 

