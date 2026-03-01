from shutil import make_archive
from os.path import join, getcwd, chdir
chdir(".."))
make_archive(f'Examples32.zip', 'zip', join(getcwd(), "bin", "Win32", "Release Examples"))
make_archive(f'Examples64.zip', 'zip', join(getcwd(), "bin", "x64", "Release Examples"))
