#!/usr/bin/env python3

import sys
import pathlib

def main():
    if len(sys.argv) != 3:
        print("usage: %s <input.txt> <output.h>" % sys.argv[0])
        exit(-1)

    _, input_file, output_file = sys.argv

    try:
        with open(input_file, "r") as f:
            lines = f.readlines()
    except IOError:
        print("failed to read input")
        exit(-1)

    hmacs_to_file = dict()

    for line in lines:
        if not line:
            continue

        splitted = line.split(":")

        if len(splitted) != 2:
            print("bad line: %s" % line)
            exit(-1)

        file, hmac = splitted

        try:
            hmac_decoded = int(hmac, 16)
        except ValueError:
            print("bad hmac: %s" % hmac)
            exit(-1)

        hmacs_to_file[hmac_decoded] = file

    sorted_hmacs = sorted(hmacs_to_file.keys())

    result = str()

    header_upper = pathlib.PurePath(output_file).stem.upper() + "_H"

    result += \
        "#ifndef %s\n#define %s\n" % (header_upper, header_upper)
    
    result += "\n"

    result += "#include <iboot.h>\n"

    result += "\n"

    result += "static const iboot_hmac_config_t iboot_hmac_config[] = {\n"

    for hmac in sorted_hmacs:
        result += "    {0x%016x, \"%s\"},\n" % (hmac, hmacs_to_file[hmac])

    result += "};\n"

    result += "\n"

    result += "static const size_t iboot_hmac_config_count = sizeof(iboot_hmac_config) / sizeof(iboot_hmac_config_t);\n"

    result += "\n"

    result += "#endif\n"


    try:
        with open(output_file, "w") as f:
            f.write(result)
    except IOError:
        print("failed to write output")
        exit(-1)
    

if __name__ == "__main__":
    main()
