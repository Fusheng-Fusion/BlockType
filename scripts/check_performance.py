#!/usr/bin/env python3
"""
Performance Regression Checker for BlockType Compiler

This script runs performance tests and checks for regressions against
baseline values defined in docs/PERFORMANCE_BASELINE.md.

Usage:
    python3 scripts/check_performance.py [--verbose] [--update-baseline]

Exit codes:
    0 - All tests passed, no regressions
    1 - Performance regression detected
    2 - Test execution error
"""

import subprocess
import sys
import re
import os
from pathlib import Path
from typing import Dict, List, Tuple

# Baseline values from PERFORMANCE_BASELINE.md
BASELINES = {
    'LexerPerformanceTest.TokenizeSmallFile': {
        'max_time_us': 500,
        'description': 'Small file (500 tokens)'
    },
    'LexerPerformanceTest.TokenizeMediumFile': {
        'max_time_us': 5000,
        'description': 'Medium file (10k tokens)'
    },
    'LexerPerformanceTest.TokenizeLargeFile': {
        'max_time_us': 50000,
        'description': 'Large file (100k tokens)'
    },
    'HeaderSearchPerformanceTest.RepeatedLookups': {
        'min_speedup': 5.0,
        'description': 'Cache speedup'
    },
    'RegressionTest.LexerThroughputBaseline': {
        'min_throughput': 500000,
        'description': 'Lexer throughput'
    },
    'RegressionTest.HeaderSearchCacheBaseline': {
        'min_speedup': 5.0,
        'description': 'Cache speedup regression'
    }
}

class PerformanceChecker:
    def __init__(self, verbose: bool = False):
        self.verbose = verbose
        self.project_root = Path(__file__).parent.parent
        self.test_binary = self.project_root / 'build' / 'tests' / 'PerformanceTest'
        self.results: Dict[str, Dict] = {}
        self.regressions: List[str] = []
        
    def log(self, message: str):
        if self.verbose:
            print(f"  {message}")
    
    def check_test_binary(self) -> bool:
        """Check if performance test binary exists."""
        if not self.test_binary.exists():
            print(f"❌ Test binary not found: {self.test_binary}")
            print("   Please build the project first:")
            print("   mkdir -p build && cd build && cmake .. && make PerformanceTest")
            return False
        return True
    
    def run_tests(self) -> Tuple[bool, str]:
        """Run performance tests and capture output."""
        print("📊 Running performance tests...")
        
        cmd = [str(self.test_binary), '--gtest_output=xml:perf_results.xml']
        if self.verbose:
            cmd.append('--gtest_print_time=1')
        
        try:
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=300,  # 5 minute timeout
                cwd=self.project_root
            )
            return result.returncode == 0, result.stdout + result.stderr
        except subprocess.TimeoutExpired:
            return False, "Performance tests timed out (> 5 minutes)"
        except Exception as e:
            return False, f"Failed to run tests: {e}"
    
    def parse_test_output(self, output: str) -> Dict[str, Dict]:
        """Parse test output to extract performance metrics."""
        results = {}
        
        # Parse time measurements
        time_pattern = r'\[(\w+\.\w+)\].*?(\d+\.?\d*)\s*us'
        for match in re.finditer(time_pattern, output, re.MULTILINE):
            test_name = match.group(1)
            time_us = float(match.group(2))
            results[test_name] = {'time_us': time_us}
        
        # Parse throughput
        throughput_pattern = r'Throughput[:\s]+(\d+\.?\d*)\s*tokens/sec'
        for match in re.finditer(throughput_pattern, output, re.MULTILINE):
            throughput = float(match.group(1))
            # Associate with the most recent test
            if results:
                last_test = list(results.keys())[-1]
                results[last_test]['throughput'] = throughput
        
        # Parse speedup
        speedup_pattern = r'Speedup[:\s]+(\d+\.?\d*)x'
        for match in re.finditer(speedup_pattern, output, re.MULTILINE):
            speedup = float(match.group(1))
            if results:
                last_test = list(results.keys())[-1]
                results[last_test]['speedup'] = speedup
        
        return results
    
    def check_regression(self, test_name: str, metrics: Dict) -> bool:
        """Check if test results show a regression."""
        if test_name not in BASELINES:
            self.log(f"No baseline defined for {test_name}")
            return False
        
        baseline = BASELINES[test_name]
        has_regression = False
        
        # Check time constraint
        if 'max_time_us' in baseline and 'time_us' in metrics:
            max_time = baseline['max_time_us']
            actual_time = metrics['time_us']
            if actual_time > max_time:
                print(f"  ⚠️  {test_name}: Time {actual_time:.1f}us exceeds baseline {max_time}us")
                has_regression = True
            else:
                self.log(f"✓ Time: {actual_time:.1f}us <= {max_time}us")
        
        # Check throughput constraint
        if 'min_throughput' in baseline and 'throughput' in metrics:
            min_throughput = baseline['min_throughput']
            actual_throughput = metrics['throughput']
            if actual_throughput < min_throughput:
                print(f"  ⚠️  {test_name}: Throughput {actual_throughput:.0f} < baseline {min_throughput}")
                has_regression = True
            else:
                self.log(f"✓ Throughput: {actual_throughput:.0f} >= {min_throughput}")
        
        # Check speedup constraint
        if 'min_speedup' in baseline and 'speedup' in metrics:
            min_speedup = baseline['min_speedup']
            actual_speedup = metrics['speedup']
            if actual_speedup < min_speedup:
                print(f"  ⚠️  {test_name}: Speedup {actual_speedup:.1f}x < baseline {min_speedup}x")
                has_regression = True
            else:
                self.log(f"✓ Speedup: {actual_speedup:.1f}x >= {min_speedup}x")
        
        return has_regression
    
    def generate_report(self):
        """Generate performance report."""
        print("\n" + "="*60)
        print("📈 Performance Test Results")
        print("="*60)
        
        for test_name, metrics in self.results.items():
            baseline = BASELINES.get(test_name, {})
            desc = baseline.get('description', test_name)
            
            print(f"\n{desc}:")
            for metric, value in metrics.items():
                if isinstance(value, float):
                    print(f"  {metric}: {value:.2f}")
                else:
                    print(f"  {metric}: {value}")
        
        if self.regressions:
            print("\n" + "="*60)
            print("⚠️  Performance Regressions Detected:")
            print("="*60)
            for test in self.regressions:
                print(f"  • {test}")
            print("\nPlease investigate and optimize before committing.")
        else:
            print("\n" + "="*60)
            print("✅ All performance tests passed!")
            print("="*60)
    
    def run(self) -> int:
        """Main entry point."""
        if not self.check_test_binary():
            return 2
        
        success, output = self.run_tests()
        if not success:
            print(f"❌ Test execution failed:\n{output}")
            return 2
        
        self.results = self.parse_test_output(output)
        
        if not self.results:
            print("⚠️  No performance metrics found in test output")
            if self.verbose:
                print(output)
            return 2
        
        print("\nChecking against baselines...")
        for test_name, metrics in self.results.items():
            if self.check_regression(test_name, metrics):
                self.regressions.append(test_name)
        
        self.generate_report()
        
        return 1 if self.regressions else 0


def main():
    import argparse
    
    parser = argparse.ArgumentParser(
        description='Check for performance regressions'
    )
    parser.add_argument(
        '--verbose', '-v',
        action='store_true',
        help='Enable verbose output'
    )
    parser.add_argument(
        '--update-baseline',
        action='store_true',
        help='Update baseline file with current results'
    )
    
    args = parser.parse_args()
    
    checker = PerformanceChecker(verbose=args.verbose)
    exit_code = checker.run()
    
    if args.update_baseline and exit_code == 0:
        print("\n📝 Baseline update feature not yet implemented.")
        print("   Please manually update docs/PERFORMANCE_BASELINE.md")
    
    sys.exit(exit_code)


if __name__ == '__main__':
    main()
