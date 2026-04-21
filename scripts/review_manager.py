#!/usr/bin/env python3
"""
BlockType 项目审查 - 任务管理工具

功能：
1. 更新Task状态
2. 添加Issue/TechDebt记录
3. 生成统计报告
4. 导出为Excel格式（可选）
5. 从MD文件同步数据
"""

import csv
import os
from datetime import datetime
from typing import Dict, List, Optional

# 文件路径
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.dirname(SCRIPT_DIR)
REVIEW_DIR = os.path.join(PROJECT_ROOT, "docs", "project_review")
DATA_DIR = os.path.join(REVIEW_DIR, "data")
REPORTS_DIR = os.path.join(REVIEW_DIR, "reports")

TASK_CSV = os.path.join(DATA_DIR, "review_task_tracker.csv")
ISSUES_CSV = os.path.join(DATA_DIR, "review_issues_log.csv")
TECH_DEBT_CSV = os.path.join(DATA_DIR, "review_tech_debt_log.csv")


class TaskManager:
    """任务管理器"""
    
    def __init__(self):
        self.tasks = self._load_csv(TASK_CSV)
    
    def _load_csv(self, filepath: str) -> List[Dict]:
        """加载CSV文件"""
        if not os.path.exists(filepath):
            return []
        
        with open(filepath, 'r', encoding='utf-8') as f:
            reader = csv.DictReader(f)
            return list(reader)
    
    def _save_csv(self, filepath: str, data: List[Dict]):
        """保存CSV文件"""
        if not data:
            return
        
        # 清理None值
        cleaned_data = []
        for row in data:
            cleaned_row = {k: (v if v is not None else '') for k, v in row.items()}
            cleaned_data.append(cleaned_row)
        
        fieldnames = cleaned_data[0].keys()
        with open(filepath, 'w', encoding='utf-8', newline='') as f:
            writer = csv.DictWriter(f, fieldnames=fieldnames)
            writer.writeheader()
            writer.writerows(cleaned_data)
    
    def update_task_status(self, task_id: str, status: str, output_file: str = ""):
        """更新任务状态"""
        now = datetime.now().strftime("%Y-%m-%d %H:%M")
        
        for task in self.tasks:
            if task['TaskID'] == task_id:
                old_status = task['Status']
                task['Status'] = status
                
                if status == "IN_PROGRESS" and not task['StartTime']:
                    task['StartTime'] = now
                
                if status == "DONE" and not task['EndTime']:
                    task['EndTime'] = now
                
                if output_file:
                    task['OutputFile'] = output_file
                
                print(f"✅ Task {task_id}: {old_status} → {status}")
                if output_file:
                    print(f"   输出文件: {output_file}")
                break
        else:
            print(f"❌ Task {task_id} not found")
            return False
        
        self._save_csv(TASK_CSV, self.tasks)
        return True
    
    def get_task_summary(self) -> Dict:
        """获取任务统计摘要"""
        summary = {
            "Total": len(self.tasks),
            "TODO": 0,
            "IN_PROGRESS": 0,
            "DONE": 0,
            "BLOCKED": 0,
        }
        
        for task in self.tasks:
            status = task.get('Status', 'TODO')
            if status in summary:
                summary[status] += 1
        
        summary["CompletionRate"] = (
            f"{summary['DONE'] / summary['Total'] * 100:.1f}%" 
            if summary['Total'] > 0 else "0%"
        )
        
        return summary
    
    def print_summary(self):
        """打印统计摘要"""
        summary = self.get_task_summary()
        print("\n" + "="*60)
        print("📊 任务统计摘要")
        print("="*60)
        print(f"总任务数:     {summary['Total']}")
        print(f"待开始:       {summary['TODO']}")
        print(f"进行中:       {summary['IN_PROGRESS']}")
        print(f"已完成:       {summary['DONE']}")
        print(f"被阻塞:       {summary['BLOCKED']}")
        print(f"完成率:       {summary['CompletionRate']}")
        print("="*60 + "\n")


class IssueManager:
    """Issue管理器"""
    
    def __init__(self):
        self.issues = self._load_csv(ISSUES_CSV)
    
    def _load_csv(self, filepath: str) -> List[Dict]:
        if not os.path.exists(filepath):
            return []
        with open(filepath, 'r', encoding='utf-8') as f:
            reader = csv.DictReader(f)
            return list(reader)
    
    def _save_csv(self, filepath: str, data: List[Dict]):
        if not data:
            return
        fieldnames = data[0].keys()
        with open(filepath, 'w', encoding='utf-8', newline='') as f:
            writer = csv.DictWriter(f, fieldnames=fieldnames)
            writer.writeheader()
            writer.writerows(data)
    
    def add_issue(self, title: str, issue_type: str, severity: str, 
                  location: str = "", description: str = ""):
        """添加新Issue"""
        now = datetime.now().strftime("%Y-%m-%d")
        issue_id = f"{len(self.issues) + 1:03d}"
        
        new_issue = {
            'IssueID': issue_id,
            'Title': title,
            'Type': issue_type,
            'Severity': severity,
            'Status': 'IDENTIFIED',
            'FoundInTask': '',
            'FoundTime': now,
            'FixedTime': '',
            'Location': location,
            'Description': description,
            'RootCause': '',
            'Impact': '',
            'FixPlan': '',
            'Dependencies': '',
            'AffectsTasks': '',
        }
        
        # 移除占位行
        self.issues = [i for i in self.issues if i['IssueID'] != '001' or i['Title']]
        self.issues.append(new_issue)
        self._save_csv(ISSUES_CSV, self.issues)
        
        print(f"✅ Issue #{issue_id} 已添加: {title}")
        return issue_id


class TechDebtManager:
    """技术债务管理器"""
    
    def __init__(self):
        self.debts = self._load_csv(TECH_DEBT_CSV)
    
    def _load_csv(self, filepath: str) -> List[Dict]:
        if not os.path.exists(filepath):
            return []
        with open(filepath, 'r', encoding='utf-8') as f:
            reader = csv.DictReader(f)
            return list(reader)
    
    def _save_csv(self, filepath: str, data: List[Dict]):
        if not data:
            return
        fieldnames = data[0].keys()
        with open(filepath, 'w', encoding='utf-8', newline='') as f:
            writer = csv.DictWriter(f, fieldnames=fieldnames)
            writer.writeheader()
            writer.writerows(data)
    
    def add_debt(self, title: str, debt_type: str, severity: str,
                 location: str = "", description: str = ""):
        """添加新技术债务"""
        now = datetime.now().strftime("%Y-%m-%d")
        debt_id = f"{len(self.debts) + 1:03d}"
        
        new_debt = {
            'DebtID': debt_id,
            'Title': title,
            'Type': debt_type,
            'Severity': severity,
            'Status': 'RECORDED',
            'Location': location,
            'FoundTime': now,
            'FixedTime': '',
            'Description': description,
            'ExpectedBehavior': '',
            'CurrentBehavior': '',
            'BlockerReason': '',
            'NextAction': '',
            'EstimatedHours': '',
            'Priority': '',
            'CompletionPercent': '0%',
            'Notes': '',
        }
        
        # 移除占位行
        self.debts = [d for d in self.debts if d['DebtID'] != '001' or d['Title']]
        self.debts.append(new_debt)
        self._save_csv(TECH_DEBT_CSV, self.debts)
        
        print(f"✅ TechDebt #{debt_id} 已添加: {title}")
        return debt_id


def generate_markdown_report():
    """从CSV生成Markdown报告"""
    task_mgr = TaskManager()
    summary = task_mgr.get_task_summary()
    
    report = f"""# BlockType 项目审查 - 进度报告

**生成时间**: {datetime.now().strftime("%Y-%m-%d %H:%M")}

## 📊 统计摘要

| 指标 | 数量 |
|------|------|
| 总任务数 | {summary['Total']} |
| 待开始 | {summary['TODO']} |
| 进行中 | {summary['IN_PROGRESS']} |
| 已完成 | {summary['DONE']} |
| 被阻塞 | {summary['BLOCKED']} |
| 完成率 | {summary['CompletionRate']} |

## 📝 任务详情

"""
    
    # 按Phase分组
    phases = {}
    for task in task_mgr.tasks:
        phase = task.get('Phase', 'Unknown')
        if phase not in phases:
            phases[phase] = []
        phases[phase].append(task)
    
    for phase, tasks in sorted(phases.items()):
        report += f"### {phase}\n\n"
        report += "| Task ID | 任务名称 | 状态 | 输出文件 |\n"
        report += "|---------|---------|------|----------|\n"
        
        for task in tasks:
            status_icon = {
                'TODO': '⬜',
                'IN_PROGRESS': '🔄',
                'DONE': '✅',
                'BLOCKED': '🚫',
            }.get(task['Status'], '❓')
            
            report += f"| {task['TaskID']} | {task['TaskName']} | {status_icon} {task['Status']} | {task.get('OutputFile', '')} |\n"
        
        report += "\n"
    
    report_path = os.path.join(REPORTS_DIR, "review_progress_report.md")
    with open(report_path, 'w', encoding='utf-8') as f:
        f.write(report)
    
    print(f"📄 进度报告已生成: {report_path}")


if __name__ == "__main__":
    import sys
    
    if len(sys.argv) < 2:
        print("用法:")
        print("  python review_manager.py summary          # 显示统计摘要")
        print("  python review_manager.py update TASK_ID STATUS [OUTPUT_FILE]")
        print("  python review_manager.py add-issue TITLE TYPE SEVERITY [LOCATION]")
        print("  python review_manager.py add-debt TITLE TYPE SEVERITY [LOCATION]")
        print("  python review_manager.py report             # 生成MD报告")
        sys.exit(1)
    
    command = sys.argv[1]
    
    if command == "summary":
        mgr = TaskManager()
        mgr.print_summary()
    
    elif command == "update":
        if len(sys.argv) < 4:
            print("用法: python review_manager.py update TASK_ID STATUS [OUTPUT_FILE]")
            sys.exit(1)
        
        task_id = sys.argv[2]
        status = sys.argv[3]
        output_file = sys.argv[4] if len(sys.argv) > 4 else ""
        
        mgr = TaskManager()
        mgr.update_task_status(task_id, status, output_file)
        mgr.print_summary()
    
    elif command == "add-issue":
        if len(sys.argv) < 5:
            print("用法: python review_manager.py add-issue TITLE TYPE SEVERITY [LOCATION]")
            sys.exit(1)
        
        title = sys.argv[2]
        issue_type = sys.argv[3]
        severity = sys.argv[4]
        location = sys.argv[5] if len(sys.argv) > 5 else ""
        
        mgr = IssueManager()
        mgr.add_issue(title, issue_type, severity, location)
    
    elif command == "add-debt":
        if len(sys.argv) < 5:
            print("用法: python review_manager.py add-debt TITLE TYPE SEVERITY [LOCATION]")
            sys.exit(1)
        
        title = sys.argv[2]
        debt_type = sys.argv[3]
        severity = sys.argv[4]
        location = sys.argv[5] if len(sys.argv) > 5 else ""
        
        mgr = TechDebtManager()
        mgr.add_debt(title, debt_type, severity, location)
    
    elif command == "report":
        generate_markdown_report()
    
    else:
        print(f"未知命令: {command}")
        sys.exit(1)
