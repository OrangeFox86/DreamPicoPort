#!/bin/env python3
import sys
import struct
import argparse
from typing import List, Union

#!/usr/bin/env python3
"""
Script to write SettingsMemory data to flash address 0x101FE000 in UF2 format
"""

# Flash memory constants
FLASH_ADDRESS = 0x10000000
SECTOR_SIZE = 4096  # Common flash sector size
PAGE_SIZE = 256     # Common flash page size

PICO1_FAMILY_ID = 0xe48bff56

UF2_MAGIC_END = 0x0AB16F30     # Randomly selected
UF2_FLAG_NOT_MAIN_FLASH = 0x00000001
UF2_FLAG_FILE_CONTAINER = 0x00001000
UF2_FLAG_FAMILY_ID_PRESENT = 0x00002000
UF2_FLAG_MD5_CHECKSUM_PRESENT = 0x00004000
UF2_BLOCK_SIZE = 512
UF2_DATA_SIZE = 256  # statically set to 256 bytes per page

class UF2Writer:
    def __init__(self, magic_start0, magic_start1, family_id):
        self.flash_address = FLASH_ADDRESS
        self.magic_start0 = magic_start0
        self.magic_start1 = magic_start1
        self.family_id = family_id

    def create_uf2_block(self, data: bytes, target_addr: int, block_num: int, total_blocks: int) -> bytes:
        """Create a single UF2 block with 256-byte data page"""
        padded_data = data + b'\x00' * (476 - len(data))
        block = struct.pack('<I', self.magic_start0)  # magicStart0
        block += struct.pack('<I', self.magic_start1)  # magicStart1
        block += struct.pack('<I', UF2_FLAG_FAMILY_ID_PRESENT)  # flags
        block += struct.pack('<I', target_addr)       # targetAddr
        block += struct.pack('<I', UF2_DATA_SIZE)     # payloadSize (always 256)
        block += struct.pack('<I', block_num)         # blockNo
        block += struct.pack('<I', total_blocks)      # numBlocks
        block += struct.pack('<I', self.family_id)    # familyID
        block += padded_data                          # data (256 bytes)
        block += struct.pack('<I', UF2_MAGIC_END)     # magicEnd
        return block

    def write_data(self, data: dict[int,bytes], output_file) -> bool:
        try:
            total_bytes = 0
            i = 0

            with open(output_file, "wb") as f:
                for addr in sorted(data.keys()):
                    start_offset = addr
                    block_data = data[addr]

                    target_addr = self.flash_address + start_offset
                    uf2_block = self.create_uf2_block(block_data, target_addr, i, len(data))
                    f.write(uf2_block)

                    total_bytes += len(block_data)

                    i += 1

            print(f"Settings written to {output_file} ({len(data)} blocks, {total_bytes} bytes)")
            print(f"Flash address: 0x{self.flash_address:08X}")
            return True

        except Exception as e:
            print(f"Error writing UF2 file: {e}")
            return False

def parse_hex_data(hex_string: str) -> bytes:
    """Parse hex string into bytes"""
    # Remove any whitespace and 0x prefixes
    clean_hex = hex_string.replace(" ", "").replace("0x", "")

    # Ensure even length
    if len(clean_hex) % 2:
        clean_hex = "0" + clean_hex

    return bytes.fromhex(clean_hex)

def parse_binary_file(filename: str) -> bytes:
    """Read binary data from file"""
    with open(filename, "rb") as f:
        return f.read()

def main():

    parser = argparse.ArgumentParser(description="Write SettingsMemory to flash in UF2 format")
    parser.add_argument("--address", type=str, default="0x50000",
                       help="Flash address (default: 0x50000)")
    parser.add_argument("--uf2", type=str, required=True, help="Path to existing UF2 file to extract settings and UF2 constants from (required)")
    parser.add_argument("--out", type=str, required=True, help="Path where output UF2 will be written (required)")
    parser.add_argument("--usb_en", type=int, default=1, help="usbEn field (bitmask, default: 1)")
    parser.add_argument("--player_enable_mode", type=str, default="2,2,2,2", help="Comma-separated playerEnableMode[4] (default: 2,2,2,2)")
    parser.add_argument("--gpio_a", type=str, default="10,12,18,20", help="Comma-separated gpioA[4] (default: 10,12,18,20)")
    parser.add_argument("--gpio_dir", type=str, default="6,7,26,27", help="Comma-separated gpioDir[4] (default: 6,7,26,27)")
    parser.add_argument("--gpio_dir_output_high", type=str, default="1,1,1,1", help="Comma-separated gpioDirOutputHigh[4] (default: 1,1,1,1)")
    parser.add_argument("--usb_led_gpio", type=int, default=25, help="usbLedGpio (default: 25)")
    parser.add_argument("--simple_usb_led_gpio", type=int, default=-1, help="simpleUsbLedGpio (default: -1)")

    args = parser.parse_args()

    try:
        # Parse flash address
        settings_address = int(args.address, 16)

        # Ingest all UF2 blocks
        with open(args.uf2, 'rb') as f:
            uf2_data = f.read()
        blocks = {}
        magic_start0 = None
        magic_start1 = None
        family_id = None
        last_addr = 0
        for i in range(0, len(uf2_data), UF2_BLOCK_SIZE):
            block = bytearray(uf2_data[i:i+UF2_BLOCK_SIZE])
            if len(block) < UF2_BLOCK_SIZE:
                continue
            (magicStart0, magicStart1, flags, targetAddr, payloadSize, blockNo, numBlocks, familyID) = struct.unpack('<8I', block[:32])
            if magic_start0 is None:
                magic_start0 = magicStart0
            if magic_start1 is None:
                magic_start1 = magicStart1
            if family_id is None:
                family_id = familyID
            last_addr = targetAddr - FLASH_ADDRESS

            blocks[targetAddr - FLASH_ADDRESS] = block[32:288]

        # SettingsMemory struct layout (use only values from args)
        magic = 0xA875EBBB
        size = 14  # number of 4-byte words starting at crc
        inv_size = (~size) & 0xFFFF
        player_enable_mode = [int(x) for x in args.player_enable_mode.split(',')]
        if len(player_enable_mode) != 4:
            raise ValueError("player_enable_mode must have 4 values")
        gpio_a = [int(x) for x in args.gpio_a.split(',')]
        if len(gpio_a) != 4:
            raise ValueError("gpio_a must have 4 values")
        gpio_dir = [int(x) for x in args.gpio_dir.split(',')]
        if len(gpio_dir) != 4:
            raise ValueError("gpio_dir must have 4 values")
        gpio_dir_output_high = [int(x) for x in args.gpio_dir_output_high.split(',')]
        if len(gpio_dir_output_high) != 4:
            raise ValueError("gpio_dir_output_high must have 4 values")
        usb_en = args.usb_en
        usb_led_gpio = args.usb_led_gpio
        simple_usb_led_gpio = args.simple_usb_led_gpio

        # Pack structure (without CRC initially)
        struct_data = struct.pack('<I', magic)  # magic
        struct_data += struct.pack('<H', size)  # size
        struct_data += struct.pack('<H', inv_size)  # invSize
        struct_data += struct.pack('<I', 0)  # crc placeholder
        struct_data += struct.pack('<I', usb_en)  # usbEn
        struct_data += struct.pack('<4B', *player_enable_mode)  # playerEnableMode[4]
        struct_data += struct.pack('<4i', *gpio_a)  # gpioA[4]
        struct_data += struct.pack('<4i', *gpio_dir)  # gpioDir[4]
        struct_data += struct.pack('<4B', *gpio_dir_output_high)  # gpioDirOutputHigh[4]
        struct_data += struct.pack('<i', usb_led_gpio)  # usbLedGpio
        struct_data += struct.pack('<i', simple_usb_led_gpio)  # simpleUsbLedGpio

        # CRC is calculated over the data starting at the crc field (i.e., struct_data[8:])
        crc_data = struct_data[8:8 + size * 4]  # size is in 4-byte words
        # Calculate CRC by XORing 4-byte chunks
        crc = 0xFFFFFFFF
        for i in range(0, len(crc_data), 4):
            chunk = crc_data[i:i+4]
            if len(chunk) == 4:
                chunk_value = struct.unpack('<I', chunk)[0]
                crc ^= chunk_value

        # Default: bbeb75a80e00f1ff1b03030301000000020202020a0000000c000000120000001400000006000000070000001a0000001b0000000101010119000000ffffffff

        # Update CRC in struct
        struct_data = struct_data[:8] + struct.pack('<I', crc) + struct_data[12:]

        # Print struct_data in hex format
        print(f"Settings data (hex): {struct_data.hex()}")

        # Overwrite the settings struct in the ingested UF2 block
        blocks[settings_address] = struct_data + b'\xFF' * (UF2_DATA_SIZE - len(struct_data))

        uf2_writer = UF2Writer(magic_start0, magic_start1, family_id)
        uf2_writer.write_data(blocks, args.out)

    except ValueError as e:
        print(f"Error: {e}")
        sys.exit(1)
    except Exception as e:
        print(f"Unexpected error: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()