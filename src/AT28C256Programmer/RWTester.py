#!/usr/bin/env python3
import sys
import time
import serial
import random
import argparse

READ = ord('r')
WRITE = ord('w')

ROMSIZE = 32768

class LY62256Tester:

    def __init__(self, 
        device, 
        reference="",
        address=0, 
        size=0, 
        mask=255,
    ):
        self.device = device
        self.address = address
        self.reference = reference
        self.size = size or ROMSIZE
        self.mask = mask
        self.buffer = bytearray(4)
        self.ok = True
        self.retention_check = True

    def __call__(self):
        self.serial = serial.Serial(
            self.device, '115200', 
            serial.EIGHTBITS,
            serial.PARITY_NONE, 
            serial.STOPBITS_ONE
        )
        self.serial.timeout = 1

        if self.reference:
            self.check_reference
        else:

            self.check_pattern(0)
            self.check_pattern(85)
            self.check_pattern(170)
            self.check_pattern(255)
            self.check_pattern()
            if self.retention_check:
                self.check_retention()

    def check_reference(self):
        with open(self.input_file, 'rb') as fd:
                rom = fd.read()
        self.ok = True
        max_address = min(self.address+self.size, ROMSIZE) 

        for address in range(self.address, max_address):
            if (address % 1024) == 0:
                print(f" > {address}")

            observed = int.from_bytes(self.read_byte(address))
            expected = rom[address]

            obs_masked = observed & self.mask
            exp_masked = expected & self.mask

            if obs_masked != exp_masked:
                print(f"- Error at {address}: {bin(obs_masked)} != {bin(exp_masked)}")
                self.ok = False

        print(f" < {address}")
        if self.ok:
            print(f"Check passed")
        else:
            print(f"Check FAILED")

    def check_pattern(self, pattern=None):
        self.ok = True
        if self.size:
            max_address = min(self.address+self.size, ROMSIZE) 
        else:
            max_address = ROMSIZE

        if pattern is None:
            print(f"Checking random pattern")
        else:
            print(f"Checking pattern {bin(pattern & self.mask)}")

        for address in range(self.address, max_address):
            if (address % 1024) == 0:
                print(f" > {address}")
            if pattern is None:
                test_pattern = random.randint(0, 255)
            else:
                test_pattern = pattern

            self.write_byte(address, test_pattern)
            received = int.from_bytes(self.read_byte(address))

            rec_masked = received & self.mask
            pat_masked = test_pattern & self.mask

            if rec_masked != pat_masked:
                print(f"- Error at {address}: {bin(rec_masked)} != {bin(pat_masked)}")
                self.ok = False

        print(f" < {address}")
        if self.ok:
            print(f"Check passed for pattern {bin(pat_masked)}")
        else:
            self.retention_check = False
            print(f"Check FAILED for pattern {bin(pat_masked)}")

    def check_retention(self):
        self.ok = True
        if self.size:
            max_address = min(self.address+self.size, ROMSIZE) 
        else:
            max_address = ROMSIZE
        test_pattern = 85
        pat_masked = test_pattern & self.mask
        print(f"Checking retention {bin(pat_masked)}: write")

        for (idx, address) in enumerate(range(self.address, max_address)):
            address += 1
            if (address % 1024) == 0:
                print(f" > {address}")

            self.write_byte(address, test_pattern)

        print(f" < {address}")
        if self.ok:
            print(f"Write passed for pattern {bin(pat_masked)}")

        print(f"Waiting for 60 seconds before reading")
        time.sleep(60)
        print(f"Checking retention {bin(pat_masked)}: read")

        for (idx, address) in enumerate(range(self.address, max_address)):
            address += 1
            if (address % 1024) == 0:
                print(f" > {address}")

            received = int.from_bytes(self.read_byte(address))

            rec_masked = received & self.mask

            if rec_masked != pat_masked:
                print(f"- Error at {address}: {bin(rec_masked)} != {bin(pat_masked)}")
                self.ok = False

        print(f" < {address}")
        if self.ok:
            print(f"Check passed for pattern {bin(pat_masked)}")
        else:
            print(f"Check FAILED for pattern {bin(pat_masked)}")

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
                print(f"Write error.")
                self.ok = False

    def read_byte(self, address):
        self.buffer[0] = READ
        self.buffer[1] = (address >> 8) & 0x7F
        self.buffer[2] = address & 0xFF
        self.serial.write(self.buffer[0:3])
        byte = self.serial.read(1)
        if byte == "":
            raise Exception("No response from programmer")
        return byte

def options():
    parser = argparse.ArgumentParser(
                description="Test a ROM or RAM chip using an Atmega32u4. "
        "The ROM or RAM must be compatible with the AT28C256 ROM chip. "
        "The program writes values to the ROM or RAM and reads them back. "
        "The observed values are checked against the values that were "
        "written. "
        "Use the -d/--device option to specify the USB serial device. "
        "The -r/--reference option can be used to specify a file to compare "
        " to the contents of the the ROM. "
        "The -a/--address option specifies the start address. This is "
        "optional and defaults to 0. "
        "The -s/--size option specifies the size of the block to test. "
        "This is optional and defaults 32KB. "
        "The optional argument -m/--mask is an 8-bit integer that represents "
        "a bit mask. Only the bits that are set in this mask are used in the "
        "comparisons."
    )

    parser.add_argument("-d", "--device", required=True, action="store",
            help="The USB serial device for accessing the programmer.")
    parser.add_argument("-r", "--reference", required=False, action="store",
            type=str, help="Compare the ROM contents against the given file.")
    parser.add_argument("-a", "--address", required=False, action="store",
            type=int, default=0, 
            help="The start address for the test.")
    parser.add_argument("-s", "--size", required=False, action="store",
            type=int, default=0, help="The size of the block to test (in bytes).")
    parser.add_argument("-m", "--mask", required=False, action="store",
            type=int, default=255, help="Compare using the provided mask.")
    return vars(parser.parse_args())

def main():
    LY62256Tester(**options())()

if __name__ == "__main__" :
    main()

