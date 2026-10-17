"""Build-time OpenCC conversion. The installed IME only reads the final SQLite."""
from pathlib import Path
import hashlib,json,sys,zipfile

def simplified_converter(root: Path):
    spec=json.loads((root/'upstream-lock.json').read_text())['build_tools']['opencc']
    archive=root/'work/language-data'/spec['filename']
    if not archive.exists():raise RuntimeError('Run scripts/fetch-language-data.py to fetch the pinned OpenCC build tool.')
    if hashlib.sha256(archive.read_bytes()).hexdigest()!=spec['sha256']:raise RuntimeError('OpenCC build tool checksum mismatch')
    folder=root/'work/tooling/opencc-python'
    folder.mkdir(parents=True,exist_ok=True)
    with zipfile.ZipFile(archive) as wheel:
        # Only extract the package; never run wheel installation hooks.
        for name in wheel.namelist():
            if name.startswith('opencc/'):
                target=(folder/name).resolve()
                if not target.is_relative_to(folder.resolve()):raise RuntimeError('Unsafe wheel path')
                target.parent.mkdir(parents=True,exist_ok=True)
                content=wheel.read(name)
                if not target.exists() or target.read_bytes()!=content:target.write_bytes(content)
        license_name=next(name for name in wheel.namelist() if name.endswith('.dist-info/LICENSE.txt'))
        notice=wheel.read('opencc/NOTICE.txt').decode()
        license_text=wheel.read(license_name).decode()
    sys.path.insert(0,str(folder))
    from opencc import OpenCC
    converter=OpenCC('t2s')
    (root/'assets/language/OpenCC-NOTICE.txt').write_text(
        'Chinese meanings converted at build time using OpenCC t2s.\n'
        'Japanese lookup forms, readings and source links are unchanged.\n'
        'https://github.com/BYVoid/OpenCC\nhttps://github.com/yichen0831/opencc-python\n'
        f'opencc-python-reimplemented {spec["version"]}; wheel SHA-256 {spec["sha256"]}\n\n'+notice+'\n'+license_text)
    return converter.convert,{'package':spec['package'],'version':spec['version'],'sha256':spec['sha256'],'profile':'t2s','fields':['meaning']}
