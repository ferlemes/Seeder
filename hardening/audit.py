"""Fail the offline artifact audit if radio APIs or proprietary RF blobs return."""
from pathlib import Path
import hashlib
import re
import subprocess
import sys
import struct
from elftools.elf.elffile import ELFFile

project = Path(__file__).resolve().parents[1]
environment = sys.argv[2] if len(sys.argv) > 2 else 'tdisplay-offline'
build = project / '.pio/build' / environment
config = (build / 'config/sdkconfig.h').read_text()
chip = 'esp32s3' if '#define CONFIG_IDF_TARGET_ESP32S3 1' in config else 'esp32'
prefix = 'xtensa-' + chip + '-elf-'
tools = Path(sys.argv[1]) if len(sys.argv) > 1 else Path('/tmp/seeder-pio/packages/toolchain-xtensa-'+chip+'/bin')
if chip == 'esp32':
    assert '#define CONFIG_ESP32_NO_BLOBS 1' in config
else:
    assert '#define CONFIG_SEEDER_NO_RADIO 1' in config
for flag in ('ESP32_WIFI_ENABLED', 'BT_ENABLED', 'SW_COEXIST_ENABLE', 'EXTERNAL_COEX_ENABLE',
             'SECURE_BOOT', 'SECURE_FLASH_ENC_ENABLED', 'ESP32_DISABLE_BASIC_ROM_CONSOLE'):
    assert not re.search(r'^#define CONFIG_' + flag + r'\s+1\b', config, re.M), flag
report = ['Seeder offline binary audit: '+environment+' ('+chip+')',
          'Wi-Fi, Bluetooth and coexistence disabled; radio component implementations excluded.',
          'No eFuse-burning security features enabled.']
deny = re.compile(r'^(?:esp_wifi_|esp_now_|esp_bt_|esp_ble_|esp_bluedroid_|esp_phy_|coex_|esp_coex_|phy_|register_chipv7_phy|wifi_|ieee80211_|ppTask|btStart|btInUse)')
blob = re.compile(r'/lib(?:phy|rtc|net80211|pp|coexist|espnow|btdm_app|core|btbb|mesh|smartconfig|wapi)\.a\b')
for name in ('firmware', 'bootloader'):
    elf = build / (name + '.elf')
    symbols = subprocess.check_output([str(tools/(prefix+'nm')), '-n', '-C', str(elf)], text=True)
    (build / (name + '.symbols.txt')).write_text(symbols)
    for line in symbols.splitlines():
        fields = line.split(maxsplit=2)
        if len(fields) == 3 and fields[1] in ('t', 'T', 'w', 'W'):
            assert not deny.search(fields[2]), line
    map_path = build / ('bootloader/bootloader.map' if name == 'bootloader' else 'seeder_offline.map')
    assert not blob.search(map_path.read_text()), 'Radio blob in '+name+' map'
    disasm = subprocess.check_output([str(tools/(prefix+'objdump')), '-d', '-C', str(elf)], text=True)
    (build / (name + '.disassembly.txt')).write_text(disasm)
    report.append(name + ': no executable symbols matching radio API denylist; no RF blobs in link map.')
    image = build / (name + '.bin')
    data = image.read_bytes()
    with elf.open('rb') as stream:
        parsed = ELFFile(stream)
        assert struct.unpack_from('<I', data, 4)[0] == parsed.header['e_entry']
        offset = 24
        matched = 0
        for index in range(data[1]):
            address, length = struct.unpack_from('<II', data, offset)
            offset += 8
            payload = data[offset:offset+length]
            offset += length
            for section in parsed.iter_sections():
                start = section['sh_addr']
                low = max(address, start)
                high = min(address+length, start+section['sh_size'])
                if not section['sh_flags'] & 2 or section['sh_type'] == 'SHT_NOBITS' or low >= high:
                    continue
                actual = payload[low-address:high-address]
                expected = section.data()[low-start:high-start]
                if section.name == '.flash.appdesc':
                    assert actual[:144] == expected[:144] and actual[176:] == expected[176:]
                    assert actual[144:176] == hashlib.sha256(elf.read_bytes()).digest()
                else:
                    assert actual == expected, (name, section.name)
                matched += high-low
        report.append(name + ': ELF entry point and loaded section bytes match the flash image ('+str(matched)+' bytes).')
    report.append('SHA256 ' + hashlib.sha256(image.read_bytes()).hexdigest() + '  ' + image.name)
report += ['Scope: static symbol/map checks, not a proof of all instructions or absent RF emissions.',
           'Radio ROM address symbols may exist; no corresponding function bodies are emitted by this build.']
text = '\n'.join(report) + '\n'
(build/'offline-audit.txt').write_text(text)
print(text)
