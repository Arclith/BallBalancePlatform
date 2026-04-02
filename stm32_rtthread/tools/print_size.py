from __future__ import print_function

import sys
import subprocess

def main():
    if len(sys.argv) < 2:
        print("Usage: python print_size.py <elf_file>")
        return

    elf_file = sys.argv[1]
    
    try:
        # Call arm-none-eabi-size
        p = subprocess.Popen(['arm-none-eabi-size', '--format=berkeley', elf_file], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        stdout, stderr = p.communicate()
        # ensure text (py2 returns str, py3 returns bytes)
        if isinstance(stdout, bytes):
            stdout = stdout.decode(errors='replace')
        if isinstance(stderr, bytes):
            stderr = stderr.decode(errors='replace')
        
        if p.returncode != 0:
            print(stderr)
            return

    # also print the raw size tool output for reference (include command line like make does)
    print('')
    print('arm-none-eabi-size "{}"'.format(elf_file))
    print(stdout.strip())
    print('')

        lines = stdout.strip().splitlines()
        try:
            # Call arm-none-eabi-size
            p = subprocess.Popen(['arm-none-eabi-size', '--format=berkeley', elf_file], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            stdout, stderr = p.communicate()
            # ensure text (py2 returns str, py3 returns bytes)
            if isinstance(stdout, bytes):
                stdout = stdout.decode(errors='replace')
            if isinstance(stderr, bytes):
                stderr = stderr.decode(errors='replace')

            if p.returncode != 0:
                print(stderr)
                return

            # also print the raw size tool output for reference (include command line like make does)
            print('')
            print('arm-none-eabi-size "{}"'.format(elf_file))
            print(stdout.strip())
            print('')

            lines = stdout.strip().splitlines()
            if len(lines) < 2:
                print(stdout)
                return

            # Parse the second line
            #    text    data     bss     dec     hex filename
            #   61592    1444    4712   67748   108a4 rtthread.elf
            values = lines[1].split()
            text = int(values[0])
            data = int(values[1])
            bss = int(values[2])

            flash = text + data
            ram = data + bss

            flash_total = 128 * 1024
            ram_total = 20 * 1024

            # Use float() for Python2 compatibility
            flash_percent = (float(flash) / float(flash_total)) * 100
            ram_percent = (float(ram) / float(ram_total)) * 100

            print("----------------------------------------------------------------")
            print("Flash: {} bytes ({:.2f} KB) - {:.2f}%".format(flash, flash/1024.0, flash_percent))
            print("RAM:   {} bytes ({:.2f} KB) - {:.2f}%".format(ram, ram/1024.0, ram_percent))
            print("----------------------------------------------------------------")

        except Exception as e:
            print("Error parsing size output: " + str(e))
