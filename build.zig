const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{
        .whitelist = &.{
            .{ .cpu_arch = .x86_64, .os_tag = .windows },
            .{ .cpu_arch = .x86_64, .os_tag = .linux },
        },
    });
    const optimize = b.standardOptimizeOption(.{ .preferred_optimize_mode = .ReleaseFast });

    const asset_preprocessor_module = b.createModule(.{
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    asset_preprocessor_module.addCSourceFiles(.{
        .files = &.{
            "src/game_asset_preprocess.c",
        },
    });

    const asset_preprocesser_executable = b.addExecutable(.{
        .name = "AssetPreprocess",
        .root_module = asset_preprocessor_module,
    });

    const game_module = b.createModule(.{
        .target = target,
        .optimize = optimize,
    });

    // NOTE: Assets.
    inline for (.{
        "assets/images/watermelon.png",
    }) |asset| {
        const run = b.addRunArtifact(asset_preprocesser_executable);
        run.addFileArg(b.path(asset));

        game_module.addIncludePath(run.addOutputFileArg(b.fmt("{s}.h", .{std.fs.path.basename(asset)})).dirname());
    }

    game_module.addCSourceFiles(.{
        .files = &.{
            "src/game.c",
            "src/game_png.c",
            "src/game_ttf.c",
            "src/game_rectangle_pack.c",
        },
    });

    const game_library = b.addLibrary(.{
        // NOTE: This logic here is for a nicer library name. Since on Linux `lib` is appended to the library name as suffix
        // `libGame` does not look good.
        .name = switch (target.result.os.tag) {
            .windows => "Game",
            .linux => "game",

            else => unreachable,
        },
        .root_module = game_module,
        .linkage = .dynamic,
    });
    b.installArtifact(game_library);

    const main_module = b.createModule(.{
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });

    main_module.addCSourceFiles(.{
        .files = switch (target.result.os.tag) {
            .windows => &.{
                "src/win32.c",
                "src/win32_d3d12.c",
            },
            .linux => &.{
                "src/linux.c",
            },

            else => unreachable,
        },
    });

    const main_executable = b.addExecutable(.{
        .name = "Game",
        .root_module = main_module,
    });
    b.installArtifact(main_executable);

    switch (target.result.os.tag) {
        .windows => {},

        .linux => {
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

            main_module.linkSystemLibrary("wayland-client", .{});
            main_module.linkSystemLibrary("asound", .{});
        },

        else => unreachable,
    }
    // TODO: run step
}
