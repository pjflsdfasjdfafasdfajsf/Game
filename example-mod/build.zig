const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{ .default_target = .{ .os_tag = .freestanding, .cpu_arch = .wasm32 } });
    const optimize = b.standardOptimizeOption(.{});

    const exe = b.addExecutable(.{
        .name = "example_mod",
        .root_module = b.createModule(.{
            .target = target,
            .optimize = optimize,
            .pic = true,
        }),
    });
    exe.entry = .disabled;
    exe.rdynamic = true;

    exe.root_module.addCSourceFiles(.{
        .root = b.path("src/"),
        .files = &.{
            "main.c",
        },
    });

    exe.root_module.addIncludePath(b.path("../include/"));

    b.installArtifact(exe);

    const run_step = b.step("run", "Run the app");
    const run_cmd = b.addRunArtifact(exe);
    run_step.dependOn(&run_cmd.step);
    run_cmd.step.dependOn(b.getInstallStep());
    if (b.args) |args| run_cmd.addArgs(args);
}
