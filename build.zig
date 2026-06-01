// TODO:
// * Have all assets merged into one game_assets.h so that game.c can just #include "game_assets.h", since these arrays embedding
// assets in the middle of code look rather ugly.
const std = @import("std");

const game_source_files: []const []const u8 = &.{
    "src/game.c",
    "src/game_png.c",
    "src/game_ttf.c",
    "src/game_rectangle_pack.c",
};

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{
        .whitelist = &.{
            .{ .cpu_arch = .x86_64, .os_tag = .windows },
            .{ .cpu_arch = .x86_64, .os_tag = .linux },
        },
    });
    const optimize = b.standardOptimizeOption(.{ .preferred_optimize_mode = .ReleaseFast });

    const asset_preprocessor_module = b.createModule(.{
        // NOTE: this must be built for the host
        .target = b.graph.host,
        .optimize = optimize,
        .link_libc = true,
    });
    asset_preprocessor_module.addCSourceFiles(.{
        .files = &.{
            "src/game_asset_preprocess.c",
        },
    });
    const asset_preprocessor_executable = b.addExecutable(.{
        .name = "AssetPreprocess",
        .root_module = asset_preprocessor_module,
    });

    const main_module = b.createModule(.{
        .target = target,
        .optimize = optimize,
        .sanitize_c = .off,
    });
    if (optimize == .Debug) {
        main_module.addCMacro("DEBUG", "1");
    }
    main_module.addCSourceFiles(.{
        .files = switch (target.result.os.tag) {
            .windows => &.{ "src/win32.c", "src/win32_d3d12.c" },
            .linux => &.{ "src/linux.c", "src/linux_vulkan.c" },
            else => unreachable,
        },
    });

    const main_executable = b.addExecutable(.{
        .name = "Game",
        .root_module = main_module,
    });
    b.installArtifact(main_executable);

    switch (target.result.os.tag) {
        .windows => {
            // NOTE: This is for cross-compilation
            const dxc = switch (b.graph.host.result.os.tag) {
                .windows => (b.lazyDependency("dxc-win32", .{}) orelse return).path("bin/x64/dxc.exe"),
                .linux => (b.lazyDependency("dxc-linux", .{}) orelse return).path("bin/dxc"),
                else => unreachable,
            };

            inline for (.{
                .{ "src/hlsl/BasicGeometry.hlsl", "VSMain", "vs_6_6", "BasicGeometryVS.h" },
                .{ "src/hlsl/BasicGeometry.hlsl", "PSMain", "ps_6_6", "BasicGeometryPS.h" },
            }) |shader| {
                // NOTE: would have used b.addSystemCommand if it did not require initial argv[0]
                const dxc_run_step = std.Build.Step.Run.create(b, "dxc");
                dxc_run_step.addFileArg(dxc);
                dxc_run_step.addArgs(&.{ "-T", shader[2], "-E", shader[1], "-Fh" });
                main_module.addIncludePath(dxc_run_step.addOutputFileArg(shader[3]).dirname());
                dxc_run_step.addFileArg(b.path(shader[0]));
            }

            main_module.addWin32ResourceFile(.{
                .file = b.path("src/win32/app.rc"),
            });

            main_module.linkSystemLibrary("kernel32", .{ .use_pkg_config = .no });
            main_module.linkSystemLibrary("user32", .{ .use_pkg_config = .no });
            main_module.linkSystemLibrary("ole32", .{ .use_pkg_config = .no });
            main_module.linkSystemLibrary("dxgi", .{ .use_pkg_config = .no });
            main_module.linkSystemLibrary("dxguid", .{ .use_pkg_config = .no });
            main_module.linkSystemLibrary("d3d12", .{ .use_pkg_config = .no });
            // NOTE: This MUST be below these! Some of the libraries above are apparently 'libc names' which automatically
            // sets link_libc to `true`.
            main_module.link_libc = false;
            // NOTE: And since we do not link libc Zig now won't provide WinAPI headers for some reason, so we have to include
            // them manually.
            const zig_library_directory = b.graph.zig_lib_directory.path orelse unreachable;
            main_module.addIncludePath(.{ .cwd_relative = b.pathJoin(&.{ zig_library_directory, "libc/include/any-windows-any" }) });
        },

        .linux => {
            // NOTE: unlike dxc, there are no prebuilt binaries for glslc so we will have to hope that user has it in their
            // PATH
            const wayland_protocols = b.run(&.{ "pkg-config", "--variable=pkgdatadir", "wayland-protocols" });
            const xdg_shell_xml = b.pathJoin(&.{ std.mem.trim(u8, wayland_protocols, "\n"), "stable/xdg-shell/xdg-shell.xml" });

            main_module.addIncludePath(xdg_shell: {
                const run = b.addSystemCommand(&.{ "wayland-scanner", "client-header", xdg_shell_xml });
                break :xdg_shell run.addOutputFileArg("xdg-shell-client-protocol.h");
            }.dirname());

            main_module.addCSourceFile(.{ .file = xdg_shell: {
                const run = b.addSystemCommand(&.{ "wayland-scanner", "private-code", xdg_shell_xml });
                break :xdg_shell run.addOutputFileArg("xdg-shell-client-protocol.c");
            } });

            inline for (.{
                .{ "src/glsl/BasicGeometry.vert", "BasicGeometry.vert.spv" },
                .{ "src/glsl/BasicGeometry.frag", "BasicGeometry.frag.spv" },
            }) |shader| {
                const glslc_run_step = b.addSystemCommand(&.{ "glslc", shader[0] });
                const compiled_spirv_path = glslc_run_step.addPrefixedOutputFileArg("-o", shader[1]);
                processAsset(b, compiled_spirv_path, shader[1], main_module, asset_preprocessor_executable);
            }

            main_module.linkSystemLibrary("wayland-client", .{});
            main_module.linkSystemLibrary("asound", .{});
        },

        else => unreachable,
    }

    if (optimize == .Debug) {
        const game_module = b.createModule(.{
            .target = target,
            .optimize = optimize,
            .sanitize_c = .off,
            .link_libc = false,
        });
        game_module.addCMacro("DEBUG", "1");
        game_module.addCSourceFiles(.{ .files = game_source_files });
        addGameAssets(b, game_module, asset_preprocessor_executable);
        addPlatformMacro(target, game_module);

        // NOTE: https://codeberg.org/ziglang/zig/issues/35559
        if (target.result.os.tag == .windows) {
            game_module.addCSourceFile(.{
                .file = b.addWriteFiles().add("dll.c",
                    \\__declspec(dllexport) int _DllMainCRTStartup(void *inst, unsigned int reason, void *reserved) {
                    \\    return 1;
                    \\}
                ),
            });
        }

        const game_library = b.addLibrary(.{
            // NOTE: This logic here is for a nicer library name. Since on Linux `lib` is prepended to the library name,
            // `libGame` does not look good.
            .name = switch (target.result.os.tag) {
                .windows => "Game",
                .linux => "game",
                else => unreachable,
            },
            .root_module = game_module,
            .linkage = .dynamic,
        });
        b.getInstallStep().dependOn(&b.addInstallArtifact(game_library, .{ .dest_dir = .{ .override = .bin } }).step);
    } else {
        main_module.addCSourceFiles(.{ .files = game_source_files });
        addGameAssets(b, main_module, asset_preprocessor_executable);
        addPlatformMacro(target, main_module);
    }

    // NOTE: This is another workaround. This time around the fact that `bear` does not work with build.zig

    const compile_flags_step = CompileFlagsStep.init(b, main_module);
    compile_flags_step.step.dependOn(&main_executable.step);
    b.getInstallStep().dependOn(&compile_flags_step.step);

    //

    const run_main_executable = b.addRunArtifact(main_executable);
    run_main_executable.step.dependOn(b.getInstallStep());
    if (b.args) |args| {
        run_main_executable.addArgs(args);
    }

    const run_step = b.step("run", "Run the game");
    run_step.dependOn(&run_main_executable.step);
}

fn addPlatformMacro(target: std.Build.ResolvedTarget, module: *std.Build.Module) void {
    module.addCMacro(switch (target.result.os.tag) {
        .windows => "WIN32",
        .linux => "LINUX",

        else => unreachable,
    }, "");
}

fn addGameAssets(b: *std.Build, module: *std.Build.Module, asset_preprocessor: *std.Build.Step.Compile) void {
    var assets_directory = b.build_root.handle.openDir(b.graph.io, "assets", .{ .iterate = true }) catch std.debug.panic("Could not open assets directory.", .{});
    defer assets_directory.close(b.graph.io);

    var assets_directory_walker = assets_directory.walk(b.allocator) catch std.debug.panic("Out of memory.", .{});
    defer assets_directory_walker.deinit();

    while (assets_directory_walker.next(b.graph.io) catch std.debug.panic("Could not walk the assets directory.", .{})) |entry| {
        if (entry.kind != .file) {
            continue;
        }

        processAsset(b, b.path(b.pathJoin(&.{ "assets", entry.path })), entry.basename, module, asset_preprocessor);
    }
}

fn processAsset(b: *std.Build, path: std.Build.LazyPath, basename: []const u8, module: *std.Build.Module, asset_preprocessor: *std.Build.Step.Compile) void {
    const asset_preprocessor_run_step = b.addRunArtifact(asset_preprocessor);
    asset_preprocessor_run_step.addFileArg(path);
    module.addIncludePath(asset_preprocessor_run_step.addOutputFileArg(b.fmt("{s}.h", .{basename})).dirname());
}

const CompileFlagsStep = struct {
    step: std.Build.Step,
    module: *std.Build.Module,

    fn init(b: *std.Build, module: *std.Build.Module) *CompileFlagsStep {
        const compile_flags = b.allocator.create(CompileFlagsStep) catch @panic("OOM");
        compile_flags.* = .{
            .step = .init(.{
                .id = .custom,
                .name = "compile_flags.txt",
                .owner = b,
                .makeFn = make,
            }),
            .module = module,
        };
        return compile_flags;
    }

    fn make(step: *std.Build.Step, _: std.Build.Step.MakeOptions) !void {
        const compile_flags: *CompileFlagsStep = @fieldParentPtr("step", step);

        const b: *std.Build = step.owner;
        const io: std.Io = b.graph.io;

        var file_write_buffer: [4096]u8 = undefined;

        const file = try std.Io.Dir.cwd().createFile(io, "compile_flags.txt", .{});
        defer file.close(io);

        var file_writer = file.writer(io, &file_write_buffer);
        defer file_writer.flush() catch {};

        for (compile_flags.module.include_dirs.items) |include_directory| {
            const lazy_path = switch (include_directory) {
                .path, .path_system, .path_after => |lazy_path| lazy_path,
                else => continue,
            };

            const cache_path = try lazy_path.getPath4(b, step);
            const full_path = b.pathResolve(&.{ cache_path.root_dir.path orelse ".", cache_path.sub_path });
            try file_writer.interface.print("-I{s}\n", .{full_path});
        }
    }
};
