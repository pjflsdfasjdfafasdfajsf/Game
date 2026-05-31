const std = @import("std");

pub fn build(b: *std.Build) void {
    // TODO: Fill in the whitelist.
    const target = b.standardTargetOptions(.{});
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
            "src/game_wav.c",
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
}
