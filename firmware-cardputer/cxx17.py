# C++17 for C++ sources only. PlatformIO has no build_flags_cxx option, and
# -std in build_flags would also hit the C sources (protocol/ocp_text.c).
Import("env")
env.Append(CXXFLAGS=["-std=gnu++17"])
