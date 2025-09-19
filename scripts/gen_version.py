Import("env")
import subprocess, os, time

def git(*args):
    try:
        return subprocess.check_output(["git"] + list(args)).decode().strip()
    except Exception:
        return "nogit"

rev  = git("rev-parse","--short","HEAD")
when = time.strftime("%Y-%m-%d %H:%M:%S")
content = f'#pragma once\n#define SEEDBOX_GIT "{rev}"\n#define SEEDBOX_BUILT "{when}"\n'
dst = os.path.join(env["PROJECT_INCLUDE_DIR"], "BuildInfo.h")
os.makedirs(os.path.dirname(dst), exist_ok=True)
with open(dst, "w") as f:
    f.write(content)
