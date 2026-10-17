#!/usr/bin/env python3
"""Adapt upstream libime to the existing Fcitx targets and embed static libs only."""
from pathlib import Path
import re
root=Path(__file__).resolve().parents[1];base=root/'work/libime'
p=base/'CMakeLists.txt';s=p.read_text()
s=s.replace('find_package(Fcitx5Utils ${REQUIRED_FCITX_VERSION} REQUIRED CONFIG)','if(NOT TARGET Fcitx5::Utils)\nfind_package(Fcitx5Utils ${REQUIRED_FCITX_VERSION} REQUIRED CONFIG)\nendif()') if 'if(NOT TARGET Fcitx5::Utils)' not in s else s
s=s.replace('include("${FCITX_INSTALL_CMAKECONFIG_DIR}/Fcitx5Utils/Fcitx5CompilerSettings.cmake")','include("${TAILKANA_ROOT}/work/fcitx5-macos/fcitx5/cmake/Fcitx5CompilerSettings.cmake")')
s=s.replace('pkg_check_modules(ZSTD REQUIRED IMPORTED_TARGET "libzstd")','''find_path(TAILKANA_ZSTD_INCLUDE zstd.h HINTS /opt/homebrew/include NO_CMAKE_FIND_ROOT_PATH REQUIRED)
find_library(TAILKANA_ZSTD_LIB zstd HINTS /opt/homebrew/lib NO_CMAKE_FIND_ROOT_PATH REQUIRED)
add_library(PkgConfig::ZSTD INTERFACE IMPORTED)
set_target_properties(PkgConfig::ZSTD PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${TAILKANA_ZSTD_INCLUDE}" INTERFACE_LINK_LIBRARIES "${TAILKANA_ZSTD_LIB}")''')
if p.read_text()!=s:p.write_text(s)
for name in ['src/libime/core/CMakeLists.txt','src/libime/pinyin/CMakeLists.txt','src/libime/table/CMakeLists.txt','tools/CMakeLists.txt']:
 p=base/name;s=re.sub(r'install\([\s\S]*?\)\s*\n','',p.read_text())
 if p.read_text()!=s:p.write_text(s)
