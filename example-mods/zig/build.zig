const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{ .default_target = .{ .os_tag = .freestanding, .cpu_arch = .wasm32 } });
    const optimize = b.standardOptimizeOption(.{});

    const sdk = b.dependency("sdk", .{ .target = target, .optimize = optimize });

    const sdk_mod = b.createModule(.{
        .root_source_file = sdk.path("sdk.zig"),
        .target = target,
        .optimize = optimize,
    });

    const exe = b.addExecutable(.{
        .name = "zig",
        .root_module = b.createModule(.{
            .root_source_file = b.path("src/root.zig"),
            .target = target,
            .optimize = optimize,
            .imports = &.{
                .{ .name = "sdk", .module = sdk_mod },
            },
            .pic = true,
        }),
    });
    exe.entry = .{ .symbol_name = "guest_initialize" }; // make sure guest_initialize is compiled
    exe.rdynamic = true;

    b.installArtifact(exe);
}
