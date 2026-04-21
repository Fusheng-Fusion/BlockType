#!/bin/bash
# Phase任务执行工作流脚本
# 
# 用法:
#   ./scripts/run_task.sh 2 2.3 "执行Task 2.3"
#
# 功能:
# 1. 更新PHASE_PROGRESS.md状态为IN_PROGRESS
# 2. 提示用户确认开始执行
# 3. 执行完成后，自动运行验证
# 4. 验证通过后，更新状态为DONE

set -e

PHASE_NUM=$1
TASK_ID=$2
TASK_DESC=$3

if [ -z "$PHASE_NUM" ] || [ -z "$TASK_ID" ]; then
    echo "❌ 用法: $0 <phase_num> <task_id> [task_description]"
    echo "示例: $0 2 2.3 \"执行Task 2.3\""
    exit 1
fi

echo "============================================================"
echo "Phase $PHASE_NUM Task $TASK_ID 执行工作流"
echo "============================================================"
echo ""
echo "任务描述: ${TASK_DESC:-未提供}"
echo ""

# 步骤1: 更新进度看板为IN_PROGRESS
echo "📝 步骤1: 更新PHASE_PROGRESS.md状态为IN_PROGRESS..."
python3 << EOF
import re
from pathlib import Path

progress_file = Path("docs/project_review/PHASE_PROGRESS.md")
content = progress_file.read_text(encoding='utf-8')

# 替换Task状态
pattern = rf'\| {TASK_ID} \| ([^|]+) \| [^|]+ \|'
replacement = f'| {TASK_ID} | \\1 | 🔄 IN_PROGRESS |'
new_content = re.sub(pattern, replacement, content)

if new_content != content:
    progress_file.write_text(new_content, encoding='utf-8')
    print(f"✅ 已更新Task {TASK_ID}状态为IN_PROGRESS")
else:
    print(f"⚠️ 未找到Task {TASK_ID}，请手动更新PHASE_PROGRESS.md")
EOF

echo ""
echo "============================================================"
echo "⚠️  重要提醒"
echo "============================================================"
echo ""
echo "在继续执行Task之前，请确认:"
echo "1. 已阅读 PROJECT_REVIEW_PLAN_V2.md 中Task $TASK_ID的要求"
echo "2. 已理解Task的输入和输出要求"
echo "3. 准备好开始执行"
echo ""
read -p "按Enter键开始执行Task $TASK_ID，或按Ctrl+C取消..."

echo ""
echo "============================================================"
echo "🚀 现在请执行Task $TASK_ID"
echo "============================================================"
echo ""
echo "执行完成后，请运行以下命令进行验证:"
echo "  python3 scripts/phase_validator.py --phase $PHASE_NUM --task $TASK_ID"
echo ""
echo "验证通过后，状态将自动更新为DONE。"
echo "============================================================"
