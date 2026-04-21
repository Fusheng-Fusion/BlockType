#!/usr/bin/env python3
"""
Parser函数完整性验证脚本

功能:
1. 从12个子任务报告中提取所有提到的Parser函数
2. 从flowchart_parser_detail.md中提取已包含的函数
3. 对比两者，找出遗漏
4. 返回验证结果

用法:
    python3 verify_parser_completeness.py
"""

import re
import os
from pathlib import Path

# 项目根目录
PROJECT_ROOT = Path(__file__).parent.parent
REPORTS_DIR = PROJECT_ROOT / "docs" / "project_review" / "reports"

def extract_parser_functions_from_reports():
    """从12个子任务报告中提取所有Parser函数"""
    parser_functions = set()
    
    for i in range(1, 13):
        report_file = REPORTS_DIR / f"task_2.2.{i}_report.md"
        if not report_file.exists():
            continue
        
        with open(report_file, 'r', encoding='utf-8') as f:
            content = f.read()
            
            # 匹配模式1: | parseXXX | ... | ParseXXX.cpp |
            pattern1 = r'\|\s*(parse[A-Z][a-zA-Z]+)\s*\|.*\|.*Parse[A-Za-z]+\.cpp'
            matches1 = re.findall(pattern1, content)
            parser_functions.update(matches1)
            
            # 匹配模式2: Parser::parseXXX
            pattern2 = r'Parser::(parse[A-Z][a-zA-Z]+)'
            matches2 = re.findall(pattern2, content)
            parser_functions.update(matches2)
    
    return sorted(parser_functions)


def extract_parser_functions_from_flowchart():
    """从flowchart_parser_detail.md中提取已包含的函数"""
    flowchart_file = REPORTS_DIR / "flowchart_parser_detail.md"
    if not flowchart_file.exists():
        return []
    
    with open(flowchart_file, 'r', encoding='utf-8') as f:
        content = f.read()
        
        # 从Mermaid图中提取
        mermaid_pattern = r'\[(parse[A-Z][a-zA-Z]+)[^\]]*\]'
        mermaid_matches = re.findall(mermaid_pattern, content)
        
        # 从表格中提取
        table_pattern = r'\|\s*(parse[A-Z][a-zA-Z]+)\s*\|'
        table_matches = re.findall(table_pattern, content)
        
        all_functions = set(mermaid_matches + table_matches)
        return sorted(all_functions)


def main():
    print("="*70)
    print("Parser函数完整性验证")
    print("="*70)
    print()
    
    # 提取期望的函数
    expected = extract_parser_functions_from_reports()
    print(f"📋 从12个子任务报告中提取的Parser函数 ({len(expected)}个):")
    for func in expected:
        print(f"  - {func}")
    print()
    
    # 提取实际的函数
    actual = extract_parser_functions_from_flowchart()
    print(f"📄 flowchart_parser_detail.md中包含的Parser函数 ({len(actual)}个):")
    for func in actual:
        print(f"  - {func}")
    print()
    
    # 对比
    missing = set(expected) - set(actual)
    extra = set(actual) - set(expected)
    
    print("="*70)
    print("验证结果:")
    print("="*70)
    
    if missing:
        print(f"❌ 遗漏 {len(missing)} 个函数:")
        for func in sorted(missing):
            print(f"  - {func}")
        print()
        print("建议: 请将这些函数添加到flowchart_parser_detail.md中")
        return False
    else:
        print("✅ 所有Parser函数都已包含，无遗漏！")
    
    if extra:
        print(f"\n⚠️ 额外包含 {len(extra)} 个函数（未在子任务报告中提到）:")
        for func in sorted(extra):
            print(f"  - {func}")
        print("这可能是辅助函数或子流程，属于正常情况。")
    
    print()
    print("="*70)
    print("验证通过！")
    print("="*70)
    return True


def verify_parser_flowchart():
    """验证Parser流程图完整性（供phase_validator调用）"""
    return main()
