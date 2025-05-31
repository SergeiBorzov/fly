import argparse
import sys
import subprocess

def generate_wayland_protocol(option, input_file, output_file):
    result = subprocess.run(
        ["wayland-scanner", option, input_file, output_file],
        capture_output=True, text=True)

def main(argv):
    argparser = argparse.ArgumentParser()
    argparser.add_argument("option")
    argparser.add_argument("input_file")
    argparser.add_argument("output_file")

    args = argparser.parse_args()
    generate_wayland_protocol(args.option, args.input_file, args.output_file)
    

if __name__ == "__main__":
    main(sys.argv[1:])
