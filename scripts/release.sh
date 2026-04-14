#!/bin/bash
# BlockType 发布脚本

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# 打印函数
info() {
  echo -e "${GREEN}[INFO]${NC} $1"
}

warn() {
  echo -e "${YELLOW}[WARN]${NC} $1"
}

error() {
  echo -e "${RED}[ERROR]${NC} $1"
  exit 1
}

# 检查 Git 状态
check_git_status() {
  if [[ -n $(git status -s) ]]; then
    error "工作目录不干净，请先提交或暂存更改"
  fi
  
  if [[ $(git rev-parse --abbrev-ref HEAD) != "main" ]]; then
    warn "不在 main 分支，是否继续？(y/n)"
    read -r response
    if [[ "$response" != "y" ]]; then
      exit 0
    fi
  fi
}

# 获取当前版本
get_current_version() {
  grep -oP 'project\(blocktype VERSION \K[0-9]+\.[0-9]+\.[0-9]+' CMakeLists.txt
}

# 更新版本号
update_version() {
  local new_version=$1
  
  # 更新 CMakeLists.txt
  sed -i.bak "s/project(blocktype VERSION [0-9]\+\.[0-9]\+\.[0-9]\+/project(blocktype VERSION ${new_version}/" CMakeLists.txt
  rm -f CMakeLists.txt.bak
  
  info "版本号已更新为 ${new_version}"
}

# 更新 CHANGELOG
update_changelog() {
  local version=$1
  local changelog_file="CHANGELOG.md"
  
  if [[ ! -f "$changelog_file" ]]; then
    cat > "$changelog_file" << EOF
# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [${version}] - $(date +%Y-%m-%d)

### Added
- Initial release

### Changed

### Fixed

### Removed
EOF
    info "已创建 CHANGELOG.md"
  else
    # 在 Unreleased 后添加新版本
    sed -i.bak "/## \[Unreleased\]/a\\
\\
## [${version}] - $(date +%Y-%m-%d)" "$changelog_file"
    rm -f "${changelog_file}.bak"
    info "CHANGELOG.md 已更新"
  fi
}

# 创建 Git tag
create_tag() {
  local version=$1
  local tag_name="v${version}"
  
  info "创建 Git tag: ${tag_name}"
  git add CMakeLists.txt CHANGELOG.md
  git commit -m "chore: release ${tag_name}"
  git tag -a "${tag_name}" -m "Release ${tag_name}"
  
  info "Tag ${tag_name} 已创建"
}

# 构建发布版本
build_release() {
  local version=$1
  
  info "构建 Release 版本..."
  ./scripts/build.sh Release
  
  info "运行测试..."
  ./scripts/test.sh build-release
  
  info "构建完成"
}

# 打包发布
package_release() {
  local version=$1
  local package_name="blocktype-${version}"
  
  info "打包发布版本..."
  
  # 创建临时目录
  mkdir -p "dist/${package_name}"
  
  # 复制二进制文件
  cp -r build-release/bin "dist/${package_name}/"
  
  # 复制文档
  cp README.md "dist/${package_name}/"
  cp LICENSE "dist/${package_name}/" 2>/dev/null || true
  
  # 打包
  cd dist
  tar -czf "${package_name}-$(uname -s)-$(uname -m).tar.gz" "${package_name}"
  cd ..
  
  info "发布包已创建: dist/${package_name}-$(uname -s)-$(uname -m).tar.gz"
}

# 推送到远程
push_to_remote() {
  local tag_name=$1
  
  info "推送到远程仓库..."
  git push origin main
  git push origin "${tag_name}"
  
  info "已推送到远程仓库"
}

# 主函数
main() {
  local action=${1:-"help"}
  
  case "$action" in
    "patch"|"minor"|"major")
      check_git_status
      
      current_version=$(get_current_version)
      IFS='.' read -r major minor patch <<< "$current_version"
      
      case "$action" in
        "patch")
          patch=$((patch + 1))
          ;;
        "minor")
          minor=$((minor + 1))
          patch=0
          ;;
        "major")
          major=$((major + 1))
          minor=0
          patch=0
          ;;
      esac
      
      new_version="${major}.${minor}.${patch}"
      
      info "当前版本: ${current_version}"
      info "新版本: ${new_version}"
      
      warn "确认发布 ${new_version}? (y/n)"
      read -r response
      if [[ "$response" != "y" ]]; then
        exit 0
      fi
      
      update_version "$new_version"
      update_changelog "$new_version"
      create_tag "$new_version"
      build_release "$new_version"
      package_release "$new_version"
      
      info "发布准备完成！"
      info "运行以下命令推送到远程："
      echo "  git push origin main"
      echo "  git push origin v${new_version}"
      echo "  gh release create v${new_version} dist/blocktype-${new_version}-*.tar.gz"
      ;;
      
    "build")
      version=$(get_current_version)
      build_release "$version"
      package_release "$version"
      ;;
      
    "push")
      version=$(get_current_version)
      push_to_remote "v${version}"
      ;;
      
    "help"|*)
      cat << EOF
BlockType 发布脚本

用法:
  $0 <command>

命令:
  patch    发布补丁版本 (x.y.Z)
  minor    发布次要版本 (x.Y.0)
  major    发布主要版本 (X.0.0)
  build    构建当前版本的发布包
  push     推送当前版本到远程仓库
  help     显示此帮助信息

示例:
  $0 patch    # 0.1.0 -> 0.1.1
  $0 minor    # 0.1.0 -> 0.2.0
  $0 major    # 0.1.0 -> 1.0.0
EOF
      ;;
  esac
}

main "$@"
