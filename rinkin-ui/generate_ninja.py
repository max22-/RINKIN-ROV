import ninja_syntax
import os
import sys
import glob


# TODO:
# - debug / release for linux
# - put back -j

targets = {
    "linux": {
        "exe": "rinkin",
        "ffmpeg-extra-flags": [],
        "ldflags": [],
        "lua-target": "linux",
    },
    "windows": {
        "exe": "rinkin.exe",
        "prefix": "x86_64-w64-mingw32-",
        "ffmpeg-extra-flags": [
            '--target-os=mingw32',
	        '--arch=x86_64',
	        '--enable-cross-compile',
	        '--cross-prefix=x86_64-w64-mingw32-'
        ],
        "ldflags": [
            "-lws2_32",
            "-lgdi32",
            "-lopengl32",
            "-lwinmm",
            "-lncrypt",
            "-lsecur32",
            "-lcrypt32",
            "-static",
            "-static-libstdc++",
            "-static-libgcc"
        ],
        "lua-target": "mingw"
    },
    "rpi": {
        "exe": "rinkin",
        "ffmpeg-extra-flags": [],
        "prefix": "aarch64-linux-gnu-",
        "ffmpeg-extra-flags": [
            '--target-os=linux',
	        '--arch=aarch64',
	        '--enable-cross-compile',
	        '--cross-prefix=aarch64-linux-gnu-'
        ],
        "ldflags": [],
        "lua-target": "linux"
    }
}

ffmpeg_configure_flags = [
    '--disable-everything',
    '--disable-autodetect',
    '--disable-avdevice',
    '--disable-avfilter',
    '--disable-swresample',
    '--disable-programs',
    '--disable-doc',
    '--disable-debug',
    '--disable-iconv',
    '--enable-small',
    '--enable-static',
    '--disable-shared',

    '--enable-avformat',
    '--enable-avcodec',
    '--enable-avutil',
    '--enable-swscale',
    '--disable-swscale-alpha',
    '--disable-pixelutils',

    '--enable-network',
    '--enable-demuxer=rtsp',
    '--enable-protocol=file',
    '--enable-protocol=http',
    '--enable-protocol=tcp',
    '--enable-protocol=udp',
    '--enable-protocol=rtp',

    '--enable-decoder=h264',
    '--enable-decoder=hevc',
    '--enable-parser=h264',
    '--enable-parser=hevc',

    '--enable-muxer=mp4',
    '--extra-cflags="-Os -fPIC"',
    '--extra-ldflags="-s"',
]

libs = [
    "raylib",
    "lua-5.4.8",
    "ffmpeg-8.0",
    "zlib",
]



srcs = glob.glob("src/**/*.cpp", recursive=True)
srcs += ["lib/imgui/" + f for f in ["imgui.cpp", "imgui_demo.cpp", "imgui_draw.cpp", "imgui_tables.cpp", "imgui_widgets.cpp"]]
srcs += ["lib/imgui/misc/cpp/imgui_stdlib.cpp"]
srcs += ["lib/rlImGui/rlImGui.cpp"]
srcs += ["lib/implot/" + f for f in ["implot.cpp", "implot_items.cpp"]]
objs = [os.path.splitext(f)[0] + ".o" for f in srcs]
lua_scripts = glob.glob("lua/**/*.lua", recursive=True)

writer = ninja_syntax.Writer(sys.stdout)


for target_name, target in targets.items():
    raylib_build_dir = f"build/{target_name}/raylib/src"
    lua_build_dir = f"build/{target_name}/lua-5.4.8/src"
    ffmpeg_build_dir = f"build/{target_name}/ffmpeg-8.0"
    zlib_build_dir = f"build/{target_name}/zlib"

    libraylib_a = raylib_build_dir + "/libraylib.a"
    liblua_a = lua_build_dir + "/liblua.a"
    lib_ffmpeg_a = [f"build/{target_name}/ffmpeg-8.0/" + a for a in [
        'libavformat/libavformat.a',
        'libavcodec/libavcodec.a',
        'libswscale/libswscale.a',
        'libavutil/libavutil.a'
    ]]
    libz_a = zlib_build_dir + "/libz.a"
    static_libs = [libraylib_a, liblua_a] + lib_ffmpeg_a + [libz_a] # the order is important for ffmpeg and zlib

    include_dirs = ["-I" + d for d in ["lib/imgui/", "lib/raylib/src", "lib/raylib/examples/shaders", "lib/rlImGui", "lib/lua-5.4.8/src", "lib/ffmpeg-8.0", ffmpeg_build_dir, "lib/implot"]]
    cpp_flags = ["-std=c++11", "-pedantic", "-Wall"] + include_dirs
    cpp_flags_debug = cpp_flags + ["-g"]
    cpp_flags_release = cpp_flags + ["-DNDEBUG", "-Os"]
    #writer.variable("cpp_flags_debug", " ".join(cpp_flags_debug))
    writer.variable(f"cpp_flags_{target_name}", " ".join(cpp_flags_release))

    try:
        cc = f"{target['prefix']}gcc"
        cxx = f"{target['prefix']}g++"
        ar = f"{target['prefix']}ar"
        ranlib = f"{target['prefix']}ranlib"
        strip = f"{target['prefix']}strip"
        writer.variable(f"cc_{target_name}", cc)
        writer.variable(f"cxx_{target_name}", cxx)
    except KeyError:
        cc = "gcc"
        cxx = "g++"
        ar = "ar"
        ranlib = "ranlib"
        strip = "strip"
        writer.variable(f"cc_{target_name}", cc)
        writer.variable(f"cxx_{target_name}", cxx)

    

    

    #writer.rule("cpp_debug", "$cxx $cpp_flags_debug -MD -MF $out.d $cflags -c -o $out $in", depfile="$out.d")
    writer.rule(f"cpp_release_{target_name}", f"$cxx_{target_name} $cpp_flags_{target_name} -MD -MF $out.d $cflags -c -o $out $in", depfile="$out.d")
    writer.rule(f"link_{target_name}", f"$cxx_{target_name} -o $out $in $ldflags && {strip} $out")
    writer.rule(f"make_{target_name}", f"make CC={cc} CXX={cxx} $makefile $ar RANLIB={ranlib} -j8 -C $dir $target $options", restat = True)
    writer.rule(f"configure_{target_name}", "mkdir -p $build_dir && cd $build_dir && $env_vars $cmd $flags && touch configure.stamp", generator=True)
    writer.rule(f"copy_{target_name}", f"cp -rn $in build/{target_name}", generator=True)
    zip_path = f"bin/{target_name}/rinkin-{target_name}.zip"
    writer.rule(f"zip_{target_name}", f"zip -j {zip_path} $in && zip -r {zip_path} $lua_scripts", generator=True)

    writer.build(raylib_build_dir, f"copy_{target_name}", "lib/raylib")
    writer.build(lua_build_dir, f"copy_{target_name}", "lib/lua-5.4.8")

    configure_flags = " ".join(ffmpeg_configure_flags + target["ffmpeg-extra-flags"])
    writer.build(f"{ffmpeg_build_dir}/configure.stamp", f"configure_{target_name}", inputs=None, variables={"build_dir": ffmpeg_build_dir, "cmd": "../../../lib/ffmpeg-8.0/configure", "flags": configure_flags})
    writer.build(lib_ffmpeg_a, f"make_{target_name}", f"{ffmpeg_build_dir}/configure.stamp", variables={"dir": ffmpeg_build_dir})

    for src, obj in zip(srcs, objs):
        #writer.build("build/debug/" + obj, "cpp_debug", src)
        implicit = f"{ffmpeg_build_dir}/configure.stamp" if "lua_video.c" in src else None
        writer.build(f"build/{target_name}/" + obj, f"cpp_release_{target_name}", src, implicit=implicit)
    #writer.build("bin/debug/rinkin", "link", ["build/debug/" + o for o in objs] + [libraylib_a, liblua_a] + lib_ffmpeg_a + [libz_a])
    writer.build(f"bin/{target_name}/{target['exe']}", f"link_{target_name}", [f"build/{target_name}/" + o for o in objs] + static_libs, variables={"ldflags": " ".join(target["ldflags"])})
    writer.build(f"bin/{target_name}/rinkin-{target_name}.zip", f"zip_{target_name}", f"bin/{target_name}/{target['exe']}", variables={"lua_scripts": lua_scripts}, implicit=lua_scripts)

    makefile_options = ["PLATFORM=PLATFORM_DESKTOP"]
    if target_name == "windows":
        makefile_options.append("PLATFORM_OS=WINDOWS")
    elif target_name == "rpi":
        makefile_options.append("GRAPHICS=GRAPHICS_API_OPENGL_21")
    writer.build(libraylib_a, f"make_{target_name}", raylib_build_dir, variables={"dir": raylib_build_dir, "target": " ".join(makefile_options)})
    writer.build(liblua_a, f"make_{target_name}", lua_build_dir, variables={"dir": lua_build_dir, "target": target["lua-target"], "ar": f'AR="{ar} rcu"'})

    # zlib

    if target_name != "windows":
        try:
            writer.build(f"{zlib_build_dir}/configure.stamp", f"configure_{target_name}", variables={"build_dir": zlib_build_dir, "env_vars": f"CROSS_PREFIX={target['prefix']}", "cmd": "../../../lib/zlib/configure", "flags": ""})
        except KeyError:
            writer.build(f"{zlib_build_dir}/configure.stamp", f"configure_{target_name}", variables={"build_dir": zlib_build_dir, "cmd": "../../../lib/zlib/configure", "flags": ""})
        writer.build(libz_a, f"make_{target_name}", variables={"dir": zlib_build_dir}, implicit=f"{zlib_build_dir}/configure.stamp")
    else:
        writer.build(zlib_build_dir, f"copy_{target_name}", "lib/zlib")
        writer.build(libz_a, f"make_{target_name}", variables={"dir": zlib_build_dir, "makefile": "-f win32/Makefile.gcc", "options": f"PREFIX={target['prefix']}"}, implicit=zlib_build_dir)
