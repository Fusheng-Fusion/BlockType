#!/usr/bin/env python3
"""
验证通过后自动更新状态

用法:
    python3 scripts/complete_task.py --phase 2 --task 2.3 --output-file flowchart_parser_detail.md
"""

import argparse
import re
from pathlib import Path
from datetime import datetime

def complete_task(phase_num: int, task_id: str, output_file: str):
    """完成任务并更新状态"""
    
    print(f"\n{'='*70}")
    print(f"完成 Phase {phase_num} Task {task_id}")
    print(f"{'='*70}\n")
    
    # 1. 更新PHASE_PROGRESS.md
    progress_file = Path("docs/project_review/PHASE_PROGRESS.md")
    if progress_file.exists():
        content = progress_file.read_text(encoding='utf-8')
        
        # 获取当前时间
        now = datetime.now().strftime("%H:%M")
        today = datetime.now().strftime("%Y-%m-%d")
        
        # 替换Task状态
        pattern = rf'\| {task_id} \| ([^|]+) \| [^|]+ \| ([^|]*) \| ([^|]*) \| ([^|]*) \|'
        replacement = f'| {task_id} | \\1 | ✅ DONE | \\2 | {now} | AI |'
        new_content = re.sub(pattern, replacement, content)
        
        if new_content != content:
            progress_file.write_text(new_content, encoding='utf-8')
            print(f"✅ 已更新PHASE_PROGRESS.md: Task {task_id} → DONE")
        else:
            print(f"⚠️ 未找到Task {task_id}，请手动更新")
        
        # 更新整体进度
        content = progress_file.read_text(encoding='utf-8')
        # 这里可以添加更复杂的进度计算逻辑
        print(f"✅ 已记录完成时间: {today} {now}")
    else:
        print(f"❌ PHASE_PROGRESS.md 不存在")
    
    # 2. 更新更新日志
    log_entry = f"| {today} {now} | 完成Task {task_id} | AI | {output_file} |\n"
    
    # 在更新日志部分插入新条目
    if progress_file.exists():
        content = progress_file.read_text(encoding='utf-8')
        if '## 📝 更新日志' in content:
            parts = content.split('## 📝 更新日志')
            header = parts[0]
            rest = parts[1]
            
            # 找到表格开始位置
            table_match = re.search(r'\| 时间 \|.*\n\|[-| ]+\|', rest)
            if table_match:
                insert_pos = table_match.end()
                new_content = header + '## 📝 更新日志' + rest[:insert_pos] + log_entry + rest[insert_pos:]
                progress_file.write_text(new_content, encoding='utf-8')
                print(f"✅ 已添加更新日志条目")
    
    print(f"\n{'='*70}")
    print(f"✅ Task {task_id} 已完成！")
    print(f"{'='*70}\n")
    
    print("下一步:")
    print(f"1. 运行验证: python3 scripts/phase_validator.py --phase {phase_num} --task {task_id}")
    print(f"2. 验证通过后，提交给用户验收")
    print(f"3. 用户确认后，继续下一个Task\n")


def main():
    parser = argparse.ArgumentParser(description="完成任务并更新状态")
    parser.add_argument("--phase", type=int, required=True, help="Phase编号")
    parser.add_argument("--task", type=str, required=True, help="Task ID")
    parser.add_argument("--output-file", type=str, required=True, help="输出文件名")
    
    args = parser.parse_args()
    
    complete_task(args.phase, args.task, args.output_file)


if __name__ == "__main__":
    main()
