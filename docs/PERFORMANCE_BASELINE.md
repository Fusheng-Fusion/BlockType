# Performance Baseline - BlockType Compiler

> **Last Updated:** 2026-04-15
> **Version:** Phase 1 Complete

This document records performance baselines for the BlockType compiler. These baselines are used to detect performance regressions during development.

---

## 📊 Lexer Performance

### Tokenization Throughput

| Test Case | Tokens | Baseline Time | Baseline Throughput |
|-----------|--------|---------------|---------------------|
| Small File | 500 | < 500 μs | > 1,000,000 tokens/sec |
| Medium File | 10,000 | < 5,000 μs | > 2,000,000 tokens/sec |
| Large File | 100,000 | < 50,000 μs | > 2,000,000 tokens/sec |

### Specific Operations

| Operation | Baseline Time | Notes |
|-----------|---------------|-------|
| Identifier Lookup | < 5 μs per 1000 keywords | Hash table lookup |
| Numeric Literal | < 10 μs per 1000 literals | Including validation |
| String Literal | < 15 μs per 1000 strings | Including escape sequences |

---

## 📊 Preprocessor Performance

### Macro Expansion

| Test Case | Baseline Time | Notes |
|-----------|---------------|-------|
| Simple Macro | < 1 μs per expansion | Object-like macros |
| Function Macro | < 5 μs per expansion | With argument substitution |
| Nested Macro | < 10 μs per expansion | 3-level nesting |

### Include Processing

| Test Case | Baseline Time | Notes |
|-----------|---------------|-------|
| Single Include | < 100 μs | File open + lex |
| Nested Includes (10 deep) | < 1,000 μs | Include chain |
| Include Guard Detection | < 50 μs | Pattern matching |

### Conditional Compilation

| Test Case | Baseline Time | Notes |
|-----------|---------------|-------|
| #ifdef/#endif | < 2 μs per pair | Condition evaluation |
| Nested #if (100 levels) | < 500 μs | Full nesting |

---

## 📊 HeaderSearch Cache Performance

### Cache Effectiveness (D13)

| Metric | Baseline | Target |
|--------|----------|--------|
| Lookup Cache Hit Rate | > 50% (typical) | > 80% (optimized) |
| Stat Cache Hit Rate | > 90% | > 95% |
| Cached Lookup Speedup | > 10x | > 20x |

### Performance Measurements

| Operation | Uncached | Cached | Speedup |
|-----------|----------|--------|---------|
| Single Header Lookup | ~50 μs | ~2 μs | 25x |
| Repeated Lookup (1000x) | ~50,000 μs | ~2,000 μs | 25x |
| Non-existent Header | ~50 μs | ~1 μs | 50x |

---

## 📊 Memory Footprint

| Structure | Size | Notes |
|-----------|------|-------|
| Token | ≤ 48 bytes | Location + kind + length + data + language |
| SourceLocation | 8 bytes | 64-bit packed (file:offset) |
| FileEntry | ~64 bytes | Metadata only |
| MacroInfo | ~128 bytes | Definition + parameters |

---

## 📈 Regression Thresholds

Performance tests will fail if metrics fall below these thresholds:

| Metric | Minimum Acceptable | Failure Threshold |
|--------|-------------------|-------------------|
| Lexer Throughput | 500,000 tokens/sec | < 500k tokens/sec |
| Cache Speedup | 5x | < 5x speedup |
| Token Size | 48 bytes | > 48 bytes |
| SourceLocation Size | 8 bytes | > 8 bytes |

---

## 🔧 Running Performance Tests

```bash
# Run all performance tests
./build/tests/PerformanceTest

# Run with verbose output
./build/tests/PerformanceTest --gtest_output=xml:perf_results.xml

# Run specific test
./build/tests/PerformanceTest --gtest_filter=LexerPerformanceTest.*
```

---

## 📝 Updating Baselines

Baselines should be updated when:

1. **Hardware improvements** - Faster machines may justify higher baselines
2. **Algorithm improvements** - Optimizations should update baselines upward
3. **Feature additions** - New features may affect baselines

### Update Process

1. Run performance tests on reference hardware
2. Record new baseline values
3. Update this document
4. Commit changes with justification

---

## 📅 Historical Data

### Phase 1 Completion (2026-04-15)

Initial baseline establishment after Phase 1 completion.

**Test Environment:**
- OS: macOS (Darwin)
- Compiler: Clang with -O2
- Build: Release

**Key Metrics:**
- Lexer throughput: ~2M tokens/sec
- HeaderSearch cache speedup: 25x
- Token size: 32 bytes
- SourceLocation size: 8 bytes

---

*This document is maintained as part of the D19 performance verification system.*
