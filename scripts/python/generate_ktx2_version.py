import sys

def main(argv):
    libktx_version = argv[0]
    libktx_default_version = argv[1]

    content = f"""#pragma once
    #define LIBKTX_VERSION "{argv[0]}"
    #define LIBKTX_DEFAULT_VERSION "{argv[1]}"
"""
    
    with open(argv[2], "w", encoding="utf-8") as f:
        f.write(content)

if __name__ == "__main__":
    main(sys.argv[1:])

