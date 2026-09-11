# PlatformIO post-build hook: copy the built binary to bin/firmware-<env>.bin
# so a release commit can carry the exact bytes that were flashed.
import shutil, os
Import("env")

def copy_bin(source, target, env):
    src = str(target[0])
    out = os.path.join(env["PROJECT_DIR"], "bin", "firmware-%s.bin" % env["PIOENV"])
    os.makedirs(os.path.dirname(out), exist_ok=True)
    shutil.copy(src, out)
    print("copied %s -> %s" % (src, out))

env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", copy_bin)
