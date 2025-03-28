
import os

def replace_all(string, allowed, replacement):
    res = ""
    for i in string:
        if i in allowed: res += i
        else: res += replacement
    return res

if __name__ == "__main__":
    with open("src/assets.h", "w") as f:
        f.write("\n// Created with bundle.py")
        if not os.path.isdir("assets"): exit(1)
        files = os.listdir("assets")
        f.write("\n// Bundled files: " + ", ".join(files) + "\n\n")
        for i in files:
            print(f"Bundling {i}")
            name = replace_all(i.upper(), "ABCDEFGHIJKLMNOPQRSTUVWXYZ", "_")
            with open(f"assets/{i}", "rb") as a:
                a.seek(0, 2)
                f.write(f"const unsigned long _{name}_LENGTH = {a.tell()};\n")
                a.seek(0, 0)
                f.write(f"char _{name}[] = " + "{ ")
                for b in a.read():
                    f.write(f"0x{hex(b)[2:].upper():0>2}, ")
                f.write("};\n\n")
                
