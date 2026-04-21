#!/usr/bin/env python3
"""
Phase完成验证框架（统一入口）

功能:
1. 根据Phase编号，自动加载对应的验证规则
2. 执行所有必需的验证检查
3. 生成详细的验证报告
4. 返回明确的通过/失败结果

用法:
    python3 scripts/phase_validator.py --phase 2 --task 2.3
    python3 scripts/phase_validator.py --phase 2 --check-all
"""

import sys
import argparse
from pathlib import Path
from datetime import datetime

# 项目根目录
PROJECT_ROOT = Path(__file__).parent.parent
REPORTS_DIR = PROJECT_ROOT / "docs" / "project_review" / "reports"

# 导入各个验证模块
sys.path.insert(0, str(PROJECT_ROOT / "scripts"))
from verify_parser_completeness import verify_parser_flowchart


class PhaseValidator:
    """Phase验证器"""
    
    def __init__(self, phase_num: int, task_id: str = None):
        self.phase_num = phase_num
        self.task_id = task_id
        self.results = []
        self.start_time = datetime.now()
    
    def add_result(self, check_name: str, passed: bool, message: str, details: str = ""):
        """添加验证结果"""
        self.results.append({
            'check': check_name,
            'passed': passed,
            'message': message,
            'details': details
        })
    
    def verify_task_2_1(self):
        """验证Task 2.1: 定义功能域"""
        print("\n" + "="*70)
        print("验证 Task 2.1: 定义功能域")
        print("="*70 + "\n")
        
        # 检查1: 输出文件是否存在
        output_file = REPORTS_DIR / f"review_task_2.1_report.md"
        if not output_file.exists():
            self.add_result(
                "输出文件存在",
                False,
                f"❌ {output_file.name} 不存在"
            )
            return False
        else:
            self.add_result(
                "输出文件存在",
                True,
                f"✅ {output_file.name} 存在 ({output_file.stat().st_size / 1024:.1f} KB)"
            )
        
        # 检查2: 是否包含功能域清单（至少20个）
        with open(output_file, 'r', encoding='utf-8') as f:
            content = f.read()
            
            # 统计功能域数量
            import re
            domain_pattern = r'### 功能域\d+: (.+?) \('
            domains_found = re.findall(domain_pattern, content)
            
            if len(domains_found) >= 20:
                self.add_result(
                    "包含足够功能域",
                    True,
                    f"✅ 定义了{len(domains_found)}个功能域"
                )
            else:
                self.add_result(
                    "包含足够功能域",
                    False,
                    f"❌ 只定义了{len(domains_found)}个功能域，至少需要20个"
                )
        
        # 检查3: 是否包含表格格式
        with open(output_file, 'r', encoding='utf-8') as f:
            content = f.read()
            if '| 功能域 |' in content or '|功能域|' in content:
                self.add_result(
                    "包含功能域表格",
                    True,
                    "✅ 报告包含功能域表格"
                )
            else:
                self.add_result(
                    "包含功能域表格",
                    False,
                    "❌ 报告缺少功能域表格"
                )
        
        return all(r['passed'] for r in self.results)
    
    def verify_task_2_3(self):
        """验证Task 2.3: 映射到流程图"""
        print("\n" + "="*70)
        print(f"验证 Task 2.{self.task_id}: 映射到流程图")
        print("="*70 + "\n")
        
        # 检查1: 输出文件是否存在
        output_file = REPORTS_DIR / f"task_2.3_flowchart_mapping.md"
        if output_file.exists():
            self.add_result(
                "输出文件存在",
                True,
                f"✅ task_2.3_flowchart_mapping.md 存在 ({output_file.stat().st_size / 1024:.1f} KB)"
            )
        else:
            self.add_result(
                "输出文件存在",
                False,
                "❌ task_2.3_flowchart_mapping.md 不存在"
            )
            return False
        
        # 检查1.5: 是否包含Task 2.1定义的所有功能域的映射
        # 从Task 2.1报告中提取功能域列表
        task_2_1_report = REPORTS_DIR / "review_task_2.1_report.md"
        if not task_2_1_report.exists():
            self.add_result(
                "覆盖所有功能域",
                False,
                "❌ review_task_2.1_report.md 不存在，无法验证功能域覆盖"
            )
        else:
            import re
            with open(task_2_1_report, 'r', encoding='utf-8') as f:
                task_2_1_content = f.read()
                domain_pattern = r'### 功能域\d+: (.+?) \('
                required_domains = re.findall(domain_pattern, task_2_1_content)
            
            # 检查task_2.3_flowchart_mapping.md是否包含这些功能域
            with open(output_file, 'r', encoding='utf-8') as f:
                content = f.read()
                
                missing_domains = []
                for domain in required_domains:
                    if domain not in content:
                        missing_domains.append(domain)
                
                if missing_domains:
                    self.add_result(
                        "覆盖所有功能域",
                        False,
                        f"❌ 缺少以下功能域的映射: {', '.join(missing_domains)}"
                    )
                else:
                    self.add_result(
                        "覆盖所有功能域",
                        True,
                        f"✅ 所有{len(required_domains)}个功能域都已映射"
                    )
        
        # 检查2: Parser详细子图是否存在
        parser_detail = REPORTS_DIR / "flowchart_parser_detail.md"
        if parser_detail.exists():
            size_kb = parser_detail.stat().st_size / 1024
            self.add_result(
                "Parser详细子图",
                True,
                f"✅ flowchart_parser_detail.md 存在 ({size_kb:.1f} KB)"
            )
            
            # 检查3: 运行Parser函数完整性验证
            print("\n🔍 运行Parser函数完整性验证...")
            parser_success = verify_parser_flowchart()
            if parser_success:
                self.add_result(
                    "Parser函数完整性",
                    True,
                    "✅ 所有Parser函数都已包含"
                )
            else:
                self.add_result(
                    "Parser函数完整性",
                    False,
                    "❌ Parser函数有遗漏，请查看上方详细列表"
                )
        else:
            self.add_result(
                "Parser详细子图",
                False,
                "❌ flowchart_parser_detail.md 不存在"
            )
        
        # 检查4: 是否包含Mermaid图
        if parser_detail.exists():
            with open(parser_detail, 'r', encoding='utf-8') as f:
                content = f.read()
                if '```mermaid' in content:
                    self.add_result(
                        "包含Mermaid图",
                        True,
                        "✅ flowchart_parser_detail.md 包含Mermaid流程图"
                    )
                else:
                    self.add_result(
                        "包含Mermaid图",
                        False,
                        "❌ flowchart_parser_detail.md 缺少Mermaid图"
                    )
        
        # 检查5: 是否包含函数清单表格
        if parser_detail.exists():
            with open(parser_detail, 'r', encoding='utf-8') as f:
                content = f.read()
                if '| 函数名 |' in content or '|函数名|' in content:
                    self.add_result(
                        "包含函数清单",
                        True,
                        "✅ flowchart_parser_detail.md 包含函数清单表格"
                    )
                else:
                    self.add_result(
                        "包含函数清单",
                        False,
                        "❌ flowchart_parser_detail.md 缺少函数清单表格"
                    )
        
        return all(r['passed'] for r in self.results)
    
    def generate_report(self):
        """生成验证报告"""
        report_file = REPORTS_DIR / f"validation_report_phase{self.phase_num}_task{self.task_id}.md"
        
        with open(report_file, 'w', encoding='utf-8') as f:
            f.write(f"# Phase {self.phase_num} Task {self.task_id} 验证报告\n\n")
            f.write(f"**验证时间**: {self.start_time.strftime('%Y-%m-%d %H:%M:%S')}\n")
            f.write(f"**验证人**: 自动化验证脚本\n\n")
            f.write("---\n\n")
            
            # 统计
            total = len(self.results)
            passed = sum(1 for r in self.results if r['passed'])
            failed = total - passed
            
            f.write(f"## 📊 验证结果汇总\n\n")
            f.write(f"- **总检查项**: {total}\n")
            f.write(f"- **通过**: {passed}\n")
            f.write(f"- **失败**: {failed}\n")
            
            if failed == 0:
                f.write(f"\n**结论**: ✅ **验证通过**\n\n")
            else:
                f.write(f"\n**结论**: ❌ **验证失败**\n\n")
            
            f.write("---\n\n")
            f.write(f"## 🔍 详细检查结果\n\n")
            
            for i, result in enumerate(self.results, 1):
                status = "✅" if result['passed'] else "❌"
                f.write(f"### {i}. {result['check']}\n\n")
                f.write(f"**状态**: {status}\n\n")
                f.write(f"{result['message']}\n\n")
                if result['details']:
                    f.write(f"**详情**:\n```\n{result['details']}\n```\n\n")
                f.write("---\n\n")
        
        print(f"\n📄 验证报告已生成: {report_file}")
        return report_file
    
    def run(self):
        """执行验证"""
        print(f"\n{'='*70}")
        print(f"Phase {self.phase_num} Task {self.task_id} 自动化验证")
        print(f"{'='*70}")
        
        # 根据task_id选择验证方法
        if self.task_id == "2.1":
            success = self.verify_task_2_1()
        elif self.task_id == "2.3":
            success = self.verify_task_2_3()
        else:
            print(f"⚠️ 未定义Task {self.task_id}的验证规则")
            return False
        
        # 生成报告
        report_file = self.generate_report()
        
        # 打印总结
        print(f"\n{'='*70}")
        print("验证总结:")
        print(f"{'='*70}")
        
        total = len(self.results)
        passed = sum(1 for r in self.results if r['passed'])
        failed = total - passed
        
        print(f"总检查项: {total}")
        print(f"通过: {passed}")
        print(f"失败: {failed}")
        print()
        
        if failed == 0:
            print("✅ 验证通过！可以继续下一步。")
        else:
            print("❌ 验证失败！请修复上述问题后再继续。")
            print(f"\n详细报告: {report_file}")
        
        print(f"{'='*70}\n")
        
        return failed == 0


def main():
    parser = argparse.ArgumentParser(description="Phase完成验证框架")
    parser.add_argument("--phase", type=int, required=True, help="Phase编号")
    parser.add_argument("--task", type=str, required=True, help="Task ID (如 2.3)")
    
    args = parser.parse_args()
    
    validator = PhaseValidator(args.phase, args.task)
    success = validator.run()
    
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
