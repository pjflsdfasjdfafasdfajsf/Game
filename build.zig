const std = @import("std");

pub fn build(b: *std.Build) void {
    const optimize = b.standardOptimizeOption(.{
        .preferred_optimize_mode = .ReleaseFast,
    });
    const target = b.standardTargetOptions(.{});

    const sdl = b.dependency("sdl", .{
        .optimize = optimize,
        .target = target,
    });

    //
    // NOTE: SDK
    //

    const sdk = b.addLibrary(.{
        .name = "SDK",
        .root_module = b.createModule(.{
            .optimize = optimize,
            .target = target,
            .link_libc = false,
        }),
        .linkage = .static,
    });
    sdk.root_module.addCSourceFiles(.{
        .files = &.{
            "Code/Public/Math.c",
            "Code/Public/Mem.c",
        },
    });

    //
    // NOTE: Host
    //

    const host = b.addExecutable(.{
        .name = "Game",
        .root_module = b.createModule(.{
            .optimize = optimize,
            .target = target,
            .link_libc = true,
        }),
    });
    host.root_module.addCSourceFiles(.{
        .files = &.{
            "Code/Host/Main.c",
            "Code/Host/SDL.c",
        },
    });
    host.root_module.addIncludePath(b.path("Code"));
    host.root_module.linkLibrary(sdl.artifact("SDL3"));
    host.root_module.linkLibrary(sdk);

    b.installArtifact(host);
}
