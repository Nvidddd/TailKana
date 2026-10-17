add_executable(TailKanaPreview MACOSX_BUNDLE "${TAILKANA_ROOT}/src/preview.swift" "${TAILKANA_ROOT}/src/preview_selftest.swift")
target_link_libraries(TailKanaPreview FcitxConfigUI SwiftFrontend SwiftNotify)
fcitx5_import_addons(TailKanaPreview REGISTRY_VARNAME getStaticAddon ADDONS beast keyboard tailkana webpanel macosfrontend notifications)
set_target_properties(TailKanaPreview PROPERTIES MACOSX_BUNDLE_INFO_PLIST "${TAILKANA_ROOT}/src/PreviewInfo.plist")
target_compile_options(TailKanaPreview PRIVATE "$<$<COMPILE_LANGUAGE:Swift>:-parse-as-library>")

target_link_options(TailKanaPreview PRIVATE "SHELL:-Xlinker -export_dynamic")
target_link_options(Fcitx5 PRIVATE "SHELL:-Xlinker -export_dynamic")

# Keep development bundles out of Spotlight and application launchers.
set_target_properties(Fcitx5 TailKanaPreview PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/apps.noindex")

# One menu implementation serves IMK and the native status icon.
target_sources(Fcitx5 PRIVATE "${TAILKANA_ROOT}/src/product_menu.swift" "${TAILKANA_ROOT}/src/product_menu_adapter.swift")
