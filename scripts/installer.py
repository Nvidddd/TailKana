#!/usr/bin/env python3
"""Build a portable macOS installer. Never install or enable an input source."""
from pathlib import Path
import subprocess, shutil, plistlib, hashlib, json
from release import VERSION, BUILD, PACKAGE_NAME
root=Path(__file__).resolve().parents[1]
work=root/'work/installer.noindex';out=root/'outputs';work.mkdir(exist_ok=True)
payload=work/'root'
if payload.exists():shutil.rmtree(payload)
ime=payload/'Library/Input Methods/TailKanaInput.app'
ime.parent.mkdir(parents=True)
shutil.copytree(root/'work/packaged.noindex/TailKanaInput.app',ime,symlinks=True)
launcher=payload/'Applications/TailKana.app';contents=launcher/'Contents'
(contents/'MacOS').mkdir(parents=True)
(contents/'Resources').mkdir()
(contents/'Helpers').mkdir()
upstream_helpers=root/'third_party/fcitx5-macos-installer'
shutil.copytree(upstream_helpers,contents/'Resources/Fcitx5-Installer-Source')
for helper in ('register_im','enable_im'):
    target=contents/'Helpers'/helper
    subprocess.run(['xcrun','swiftc','-target','arm64-apple-macos13.3','-module-cache-path',str(root/'work/swift-cache'),str(upstream_helpers/(helper+'.swift')),'-o',str(target)],check=True)
    subprocess.run(['codesign','--force','--sign','-',str(target)],check=True)
shutil.copy2(ime/'Contents/Resources/TailKana.icns',contents/'Resources/TailKana.icns')
info={'CFBundleIdentifier':'org.tailkana.launcher','CFBundleName':'TailKana','CFBundleDisplayName':'TailKana','CFBundleExecutable':'TailKana','CFBundlePackageType':'APPL','CFBundleIconFile':'TailKana.icns','CFBundleShortVersionString':VERSION,'CFBundleVersion':BUILD,'LSMinimumSystemVersion':'15.0'}
(contents/'Info.plist').write_bytes(plistlib.dumps(info))
subprocess.run(['xcrun','swiftc','-parse-as-library','-target','arm64-apple-macos13.3','-module-cache-path',str(root/'work/swift-cache'),str(root/'src/launcher.swift'),str(root/'src/input_source_registration.swift'),'-o',str(contents/'MacOS/TailKana')],check=True)
subprocess.run(['codesign','--force','--sign','-',str(launcher)],check=True)
# Do not relocate to an older preview or another copy found by LaunchServices.
components=work/'components.plist'
subprocess.run(['pkgbuild','--analyze','--root',str(payload),str(components)],check=True)
items=plistlib.loads(components.read_bytes())
for item in items:item['BundleIsRelocatable']=False;item['BundleOverwriteAction']='upgrade'
components.write_bytes(plistlib.dumps(items))
component=work/'TailKana-component.pkg'
subprocess.run(['pkgbuild','--root',str(payload),'--component-plist',str(components),'--identifier','org.tailkana.installer','--version',VERSION,'--install-location','/','--ownership','recommended',str(component)],check=True)
resources=work/'resources';resources.mkdir(exist_ok=True)
(resources/'Welcome.html').write_text('''<html><meta charset="utf-8"><body style="font-family: -apple-system; padding:20px"><h1>TailKana</h1><p>本地日语输入法：实时 AI 上下文推荐、实时假名、中文释义、拼音第三候选、自定义毛玻璃，以及末尾拼写轮盘。</p><p>安装后，在「应用程序」中打开 <b>TailKana</b>，点击「启用并切换」。</p><p>适用于 Apple Silicon Mac，macOS 15 或更高版本。</p></body></html>''')
(resources/'Conclusion.html').write_text('''<html><meta charset="utf-8"><body style="font-family: -apple-system; padding:20px"><h2>接下来</h2><ol><li>在「应用程序」中打开 <b>TailKana</b>。</li><li>点击「启用并切换」。</li><li>在备忘录等应用输入 nihongo，按空格变换、Enter 确定。</li></ol><p>AI 开启后自动预热并常驻；可在设置调整最长预测长度和参考上下文长度。点击 AI 推荐或按 Option＋Enter 选用。</p><p>候选面板的齿轮、输入菜单中的「TailKana 设置」，以及启动器的「打开设置」均可调整选项。</p><p>若系统尚未发现输入源，请注销并重新登录后重试。</p></body></html>''')
distribution=work/'Distribution.xml'
distribution.write_text('''<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
<title>TailKana 0.3.9</title>
<welcome file="Welcome.html" mime-type="text/html"/><conclusion file="Conclusion.html" mime-type="text/html"/>
<options customize="never" require-scripts="false" hostArchitectures="arm64"/>
<allowed-os-versions><os-version min="15.0"/></allowed-os-versions>
<domains enable_anywhere="false" enable_currentUserHome="false" enable_localSystem="true"/>
<choices-outline><line choice="tailkana"/></choices-outline><choice id="tailkana" visible="false"><pkg-ref id="org.tailkana.installer"/></choice>
<pkg-ref id="org.tailkana.installer" version="0.3.9" onConclusion="none">TailKana-component.pkg</pkg-ref>
</installer-gui-script>'''.replace('0.3.9',VERSION))
package=out/PACKAGE_NAME
subprocess.run(['productbuild','--distribution',str(distribution),'--package-path',str(work),'--resources',str(resources),str(package)],check=True)
report={'package':package.name,'bytes':package.stat().st_size,'sha256':hashlib.sha256(package.read_bytes()).hexdigest(),'destinations':['/Library/Input Methods/TailKanaInput.app','/Applications/TailKana.app'],'signed':'ad-hoc apps; unsigned installer','activation':'User clicks 启用并切换 in TailKana launcher. Installer itself does not activate.'}
(out/'installer-manifest.json').write_text(json.dumps(report,ensure_ascii=False,indent=2))
print(json.dumps(report,ensure_ascii=False,indent=2))
