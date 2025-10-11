from SCons.Script import Import

Import("env")

# When PlatformIO builds tests it sets the build type to "test". We only want
# the desktop entrypoint in full program builds so unit tests can supply their
# own Unity mains without clashing with src/main.cpp.
if env.GetBuildType() != "test":
    env.Append(CPPDEFINES=[("SEEDBOX_DESKTOP_ENTRY", 1)])
