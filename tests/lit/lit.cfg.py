# -*- Python -*-
"""Lit configuration for BlockType tests."""

import lit.formats
import os

# Configuration name
config.name = "BlockType"

# Test format: ShTest (shell-style tests)
config.test_format = lit.formats.ShTest(True)

# Test file suffixes
config.suffixes = ['.test']

# Test source root
config.test_source_root = os.path.dirname(__file__)

# Test execution root
config.test_exec_root = os.path.join(config.binary_dir, 'tests', 'lit')

# Substitute blocktype binary path
config.substitutions.append(
    ('%blocktype', os.path.join(config.binary_dir, 'bin', 'blocktype'))
)

# Substitute FileCheck
config.substitutions.append(('%FileCheck', 'FileCheck'))

# Substitute not
config.substitutions.append(('%not', 'not'))

# Features
config.available_features.add('blocktype')
