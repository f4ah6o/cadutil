const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    // Root path to LibreCAD
    const librecad_root = "../..";

    // libdxfrw sources
    const libdxfrw_sources = [_][]const u8{
        librecad_root ++ "/libraries/libdxfrw/src/drw_base.cpp",
        librecad_root ++ "/libraries/libdxfrw/src/drw_classes.cpp",
        librecad_root ++ "/libraries/libdxfrw/src/drw_entities.cpp",
        librecad_root ++ "/libraries/libdxfrw/src/drw_header.cpp",
        librecad_root ++ "/libraries/libdxfrw/src/drw_objects.cpp",
        librecad_root ++ "/libraries/libdxfrw/src/libdxfrw.cpp",
        librecad_root ++ "/libraries/libdxfrw/src/libdwgr.cpp",
        librecad_root ++ "/libraries/libdxfrw/src/intern/drw_dbg.cpp",
        librecad_root ++ "/libraries/libdxfrw/src/intern/drw_textcodec.cpp",
        librecad_root ++ "/libraries/libdxfrw/src/intern/dwgbuffer.cpp",
        librecad_root ++ "/libraries/libdxfrw/src/intern/dwgreader.cpp",
        librecad_root ++ "/libraries/libdxfrw/src/intern/dwgreader15.cpp",
        librecad_root ++ "/libraries/libdxfrw/src/intern/dwgreader18.cpp",
        librecad_root ++ "/libraries/libdxfrw/src/intern/dwgreader21.cpp",
        librecad_root ++ "/libraries/libdxfrw/src/intern/dwgreader24.cpp",
        librecad_root ++ "/libraries/libdxfrw/src/intern/dwgreader27.cpp",
        librecad_root ++ "/libraries/libdxfrw/src/intern/dwgutil.cpp",
        librecad_root ++ "/libraries/libdxfrw/src/intern/dxfreader.cpp",
        librecad_root ++ "/libraries/libdxfrw/src/intern/dxfwriter.cpp",
        librecad_root ++ "/libraries/libdxfrw/src/intern/rscodec.cpp",
    };

    // jwwlib sources
    const jwwlib_sources = [_][]const u8{
        librecad_root ++ "/libraries/jwwlib/src/dl_jww.cpp",
        librecad_root ++ "/libraries/jwwlib/src/dl_writer_ascii.cpp",
        librecad_root ++ "/libraries/jwwlib/src/jwwdoc.cpp",
    };

    // librecad_core sources
    const core_sources = [_][]const u8{
        "src/librecad_core.cpp",
    };

    // Common C++ flags
    const cpp_flags = [_][]const u8{
        "-std=c++17",
        "-fPIC",
        "-DNDEBUG",
    };

    // Include paths
    const include_paths = [_][]const u8{
        "include",
        librecad_root ++ "/libraries/libdxfrw/src",
        librecad_root ++ "/libraries/libdxfrw/src/intern",
        librecad_root ++ "/libraries/jwwlib/src",
    };

    // Create a root module for C++ only library (no Zig source file)
    const lib_module = b.createModule(.{
        .root_source_file = null, // C++ only, no Zig source
        .target = target,
        .optimize = optimize,
        .link_libcpp = true,
    });

    // Build static library
    const lib = b.addLibrary(.{
        .name = "recad_core",
        .root_module = lib_module,
        .linkage = .static,
    });

    // Add include paths
    for (include_paths) |path| {
        lib.addIncludePath(b.path(path));
    }

    // Add libdxfrw sources
    lib.addCSourceFiles(.{
        .files = &libdxfrw_sources,
        .flags = &cpp_flags,
    });

    // Add jwwlib sources
    lib.addCSourceFiles(.{
        .files = &jwwlib_sources,
        .flags = &cpp_flags,
    });

    // Add core sources
    lib.addCSourceFiles(.{
        .files = &core_sources,
        .flags = &cpp_flags,
    });

    // Link C++ standard library
    lib.linkLibCpp();

    b.installArtifact(lib);

    // Create a root module for shared library
    const shared_lib_module = b.createModule(.{
        .root_source_file = null, // C++ only, no Zig source
        .target = target,
        .optimize = optimize,
        .link_libcpp = true,
    });

    // Build shared library as well
    const shared_lib = b.addLibrary(.{
        .name = "recad_core",
        .root_module = shared_lib_module,
        .linkage = .dynamic,
    });

    for (include_paths) |path| {
        shared_lib.addIncludePath(b.path(path));
    }

    shared_lib.addCSourceFiles(.{
        .files = &libdxfrw_sources,
        .flags = &cpp_flags,
    });

    shared_lib.addCSourceFiles(.{
        .files = &jwwlib_sources,
        .flags = &cpp_flags,
    });

    shared_lib.addCSourceFiles(.{
        .files = &core_sources,
        .flags = &cpp_flags,
    });

    shared_lib.linkLibCpp();

    b.installArtifact(shared_lib);

    // Install header
    b.installFile("include/librecad_core.h", "include/librecad_core.h");

    // Create a module for test executable (C only)
    const test_module = b.createModule(.{
        .root_source_file = null, // C only, no Zig source
        .target = target,
        .optimize = optimize,
        .link_libcpp = true,
    });

    // Test executable
    const test_exe = b.addExecutable(.{
        .name = "test_core",
        .root_module = test_module,
    });

    test_exe.addCSourceFiles(.{
        .files = &[_][]const u8{"test/test_core.c"},
        .flags = &[_][]const u8{"-std=c11"},
    });

    test_exe.addIncludePath(b.path("include"));
    test_exe.linkLibrary(lib);
    test_exe.linkLibCpp();

    const run_test = b.addRunArtifact(test_exe);
    const test_step = b.step("test", "Run tests");
    test_step.dependOn(&run_test.step);
}
