"""Publication boundary checks; no renderer, game installation or Pillow needed."""
import hashlib
import json
from pathlib import Path
import struct
import tempfile
import unittest
import zlib

from publish_gallery import collect, stage


class PublicationTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.source = self.root / 'gallery'
        self.source.mkdir()
        def chunk(kind, body):
            return struct.pack('>I', len(body)) + kind + body + struct.pack('>I', zlib.crc32(kind+body))
        image = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB',3840,2160,8,2,0,0,0))
        image += chunk(b'IDAT', zlib.compress(bytes((3840*3+1)*2160))) + chunk(b'IEND',b'')
        self.image = 'images/01-test.png'
        scene = dict(id='01-test',file=self.image,width=3840,height=2160,bytes=len(image),sha256=hashlib.sha256(image).hexdigest())
        self.write(self.image, image)
        self.write('manifest.json', json.dumps(dict(preview=False,scenes=[scene])).encode())
        self.write('captions.csv', b'file\nimages/01-test.png\n')
        self.write('README.txt', b'Test gallery')
        self.write('index.html', b'<a href="images/01-test.png">PNG</a><img src="thumbs/01-test.jpg">')
        self.write('recipes/01-test.scene', b'view external\n')
        for name in ('thumbs/01-test.jpg','contact-sheet.jpg','contact-sheets/001-001.jpg'):
            self.write(name,b'\xff\xd8\xff\xe0\x00\x02\xff\xd9')

    def write(self, name, data):
        path = self.source / name
        path.parent.mkdir(parents=True,exist_ok=True)
        path.write_bytes(data)

    def test_only_selected_files_are_staged(self):
        for name in ('TFX.DAT','install-media/disc.bin','roms/control.rom','logs/frame.log','images/unselected.png','work/private.txt'):
            self.write(name,b'never publish')
        count, _ = stage(self.source,self.root/'site')
        actual = {str(p.relative_to(self.root/'site')) for p in (self.root/'site').rglob('*') if p.is_file()}
        self.assertEqual(actual,set(collect(self.source))|{'.nojekyll'})
        self.assertEqual(len(actual),count+1)
        self.assertFalse(any((self.root/'site'/n).exists() for n in ('TFX.DAT','logs','roms','work','install-media')))

    def test_unresolved_lfs_is_rejected(self):
        self.write(self.image,b'version https://git-lfs.github.com/spec/v1\noid sha256:000\n')
        with self.assertRaisesRegex(ValueError,'Unresolved Git LFS'):
            collect(self.source)

    def test_corrupt_original_is_rejected(self):
        with (self.source/self.image).open('ab') as f:f.write(b'corrupt')
        with self.assertRaisesRegex(ValueError,'checksum mismatch'):
            collect(self.source)

    def test_symlink_is_rejected(self):
        path=self.source/'README.txt'
        path.unlink()
        secret=self.root/'secret';secret.write_text('private')
        path.symlink_to(secret)
        with self.assertRaisesRegex(ValueError,'Symlinks'):
            collect(self.source)

    def test_page_cannot_link_outside_publication(self):
        self.write('index.html',b'<a href="logs/frame.log">log</a>')
        with self.assertRaisesRegex(ValueError,'outside the publication'):
            collect(self.source)

    def test_traversal_is_rejected(self):
        self.write('index.html',b'<a href="%2e%2e/TFX.DAT">data</a>')
        with self.assertRaisesRegex(ValueError,'escapes'):
            collect(self.source)

    def test_existing_output_is_not_merged_or_deleted(self):
        out=self.root/'site';out.mkdir();(out/'old.bin').write_bytes(b'old')
        with self.assertRaisesRegex(ValueError,'empty staging'):
            stage(self.source,out)
        self.assertEqual((out/'old.bin').read_bytes(),b'old')


if __name__=='__main__':
    unittest.main()
