#!/usr/bin/env python3
# vim: tabstop=8 expandtab shiftwidth=4 softtabstop=4

import sys
import serial
import argparse

READ = ord('r')
WRITE = ord('w')
SDP_ON = ord('e')
SDP_OFF = ord('d')

ROMSIZE = 32768

class AT28C256Programmer:

    def __init__(self, 
        device, 
        input_file="", 
        output_file="", 
        address=0, 
        size=0, 
        sdp_enable=False,
        sdp_disable=False,
    ):
        self.device = device
        self.input_file = input_file
        self.output_file = output_file
        self.address = address
        self.size = size
        self.buffer = bytearray(4)
        self.sdp_enable = sdp_enable
        self.sdp_disable = sdp_disable
        self.ok = True

    def __call__(self):
        self.serial = serial.Serial(
            self.device, '115200', 
            serial.EIGHTBITS,
            serial.PARITY_NONE, 
            serial.STOPBITS_ONE
        )
        self.serial.timeout = 1

        if self.sdp_disable:
            self.disable_sdp()

        if self.input_file:
            with open(self.input_file, 'rb') as fd:
                rom = fd.read()
            self.upload_rom(rom)
            if self.ok:
                print(f"Uploaded ROM from {self.input_file}.")
            else:
                print(f"Error in uploading ROM from {self.input_file}.")

        elif self.output_file:
            with open(self.output_file, 'wb') as fd:
                self.download_rom(fd)
            print(f"Saved download to {self.output_file}.")

        elif not self.enable_sdp:
            print("No input or output file was specified. Exiting.")
            sys.exit(1)

        if self.sdp_enable:
            self.enable_sdp()

    def upload_rom(self, rom):
        rom_size = len(rom)
        if self.size:
            max_address = min(self.address+self.size, ROMSIZE) 
        else:
            max_address = min(self.address+rom_size, ROMSIZE)

        print(f"Uploading {self.input_file}")
        for (idx, address) in enumerate(range(self.address, max_address)):
            if (address % 1024) == 0:
                print(f" > {address}")
            self.write_byte(address, rom[idx])
        print(f" < {address}")

    def write_byte(self, address, byte):
            self.buffer[0] = WRITE
            self.buffer[1] = (address >> 8) & 0x7F
            self.buffer[2] = address & 0xFF
            self.buffer[3] = byte
            self.serial.write(self.buffer)
            check_byte = self.serial.read(1)

            if check_byte == "":
                raise Exception("No response from programmer")
            if check_byte != byte.to_bytes(1, "big"):
                self.ok = False

    def download_rom(self, fd):
        if self.size:
            max_address = min(self.address+self.size, ROMSIZE) 
        else:
            max_address = ROMSIZE

        print(f"Downloading {self.address} to {max_address-1}")

        for address in range(self.address, max_address):
            if (address % 1024) == 0:
                print(f" > {address} {'OK' if self.ok else 'FAIL'}")
            fd.write(self.read_byte(address))
        print(f" < {address} {'OK' if self.ok else 'FAIL'}")

    def read_byte(self, address):
        self.buffer[0] = READ
        self.buffer[1] = (address >> 8) & 0x7F
        self.buffer[2] = address & 0xFF
        self.serial.write(self.buffer[0:3])
        byte = self.serial.read(1)
        if byte == "":
            raise Exception("No response from programmer")
        return byte

    def enable_sdp(self):
        self.buffer[0] = SDP_ON
        self.serial.write(self.buffer[0:1])
        response = self.serial.read(1)
        print(f"Enabled SDP: {response}")
        
    def disable_sdp(self):
        self.buffer[0] = SDP_OFF
        self.serial.write(self.buffer[0:1])
        response = self.serial.read(1)
        print(f"Disabled SDP: {response}")

def options():
    parser = argparse.ArgumentParser(
                description="Program an AT28C256 or compatible ROM chip "
        "using an Atmega32u4. "
        "Use the -d/--device option to specify the serial device. Use "
        "the -i/--input_file option to upload to the ROM. Use the "
        "-o/--output_file option to download from the ROM. The -a/--adress "
        "option specifies the start address. This is optional and defaults to 0. "
        "The -s/--size option specifies the size of the upload/download. "
        "This is optional and defaults to the size of input file for uploads "
        "and up to the end of the ROM's 32KB address space for downloads."
    )

    parser.add_argument("-d", "--device", required=True, action="store",
            help="The USB serial device for accessing the programmer.")
    parser.add_argument("-i", "--input_file", required=False, action="store",
            default="", help="The ROM file to be uploaded.")
    parser.add_argument("-o", "--output_file", required=False, action="store",
            default="", help="The file where to save the download.")
    parser.add_argument("-a", "--address", required=False, action="store",
            type=int, default=0, 
            help="The start address for the upload or download.")
    parser.add_argument("-s", "--size", required=False, action="store",
            type=int, default=0, help="The size of the upload or download.")
    parser.add_argument("-E", "--sdp_enable", required=False, 
            action="store_true",
            help="Enable the software data protection after uploading.")
    parser.add_argument("-D", "--sdp_disable", required=False, 
            action="store_true",
            help="Disable the software data protection before uploading.")
    return vars(parser.parse_args())

def main():
    AT28C256Programmer(**options())()

if __name__ == "__main__" :
    main()

