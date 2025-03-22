import shutil
import argparse
import sys
import os

def copy_files(in_files, out_files):
    assert len(in_files) == len(out_files)
    for i in range(0, len(out_files)):
        shutil.copy(in_files[i], out_files[i])

def main(argv):
    argparser = argparse.ArgumentParser()
    argparser.add_argument("-i", type=str, nargs="+")
    argparser.add_argument("-o", type=str, nargs="+")

    args = argparser.parse_args()

    print(f"Args {args}")
    copy_files(args.i, args.o)
    

if __name__ == "__main__":
    main(sys.argv[1:])
