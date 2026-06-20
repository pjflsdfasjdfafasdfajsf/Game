const std = @import("std");

const source_files: []const []const u8 = &.{
    "main.cpp",
    "app.cpp",
    "atlaspacker.cpp",
    "inputmap.cpp",
    "wasm.cpp",
};

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const sdl = b.dependency("sdl", .{ .target = target, .optimize = .ReleaseFast });
    const stb = b.dependency("stb", .{});
    const wasm3 = b.dependency("wasm3", .{});

    const exe = b.addExecutable(.{
        .name = "tiramisu",
        .root_module = b.createModule(.{
            .target = target,
            .optimize = optimize,
            .link_libc = true,
            .link_libcpp = true,
        }),
    });

    exe.root_module.addCSourceFiles(.{
        .root = b.path("src/"),
        .files = source_files,
    });

    exe.root_module.addIncludePath(b.path("include/"));
    exe.root_module.addIncludePath(b.path("src/"));

    exe.root_module.addAfterIncludePath(stb.path("."));

    exe.root_module.linkLibrary(sdl.artifact("SDL3"));
    exe.root_module.linkLibrary(wasm3.artifact("m3"));

    b.installArtifact(exe);

    const run_step = b.step("run", "Run the app");
    const run_cmd = b.addRunArtifact(exe);
    run_step.dependOn(&run_cmd.step);
    run_cmd.step.dependOn(b.getInstallStep());
    if (b.args) |args| run_cmd.addArgs(args);
}
