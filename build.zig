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
            "Code/Host/SDL_Renderer.c",
            "Code/Host/STB.c",
        },
    });
    host.root_module.addIncludePath(b.path("Code"));
    host.root_module.linkLibrary(sdl.artifact("SDL3"));

    b.installArtifact(host);
}
