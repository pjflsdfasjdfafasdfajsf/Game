const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{
        .preferred_optimize_mode = .ReleaseFast,
    });

    const sdl = b.dependency("sdl", .{
        .target = target,
        .optimize = optimize,
    });

    //
    // NOTE: SDK
    //

    const sdk = b.addLibrary(.{
        .name = "sdk",

        .root_module = b.createModule(.{
            .target = target,
            .optimize = optimize,
        }),
    });
    sdk.root_module.addCSourceFiles(.{
        .files = &.{
            "Code/Public/Math.c",
            "Code/Public/Mem.c",
        },
    });

    //
    // NOTE: RISCV
    //

    // NOTE: Stencils
    // A lot of flags here are for the most optimal codegen, and besides flags
    // the actual code there is also optimized for optimal codegen.
    const riscv_jit_stencils = b.addObject(.{
        .name = "riscv_jit_stencils",

        .root_module = b.createModule(.{
            .target = target,
            // NOTE: ReleaseFast adds nop padding
            .optimize = .ReleaseSmall,

            .omit_frame_pointer = true,
            // NOTE: Assumes that all symbols live in the negative address
            // so their absolute address maps to a sign-extended 32 bit value
            .code_model = .kernel,
        }),
    });
    const riscv_jit_stencils_src = b.path("Code/Host/RiscV/RiscV_JIT_Stencils.c");

    riscv_jit_stencils.root_module.addCSourceFile(.{
        .file = riscv_jit_stencils_src,
    });
    riscv_jit_stencils.root_module.addIncludePath(b.path("Code"));

    b.getInstallStep().dependOn(&b.addInstallArtifact(riscv_jit_stencils, .{
        .dest_dir = .{ .override = .bin },
    }).step);

    const riscv_jit_stencils_disasm = b.addExecutable(.{
        .name = "riscv_jit_stencils_disasm",

        .root_module = b.createModule(.{
            .target = target,
            .optimize = .ReleaseSmall,
            .link_libc = true,
        }),
    });
    riscv_jit_stencils_disasm.root_module.addCSourceFile(.{
        .file = b.path("Code/Host/RiscV/RiscV_JIT_Stencils_Disasm.c"),
    });
    riscv_jit_stencils_disasm.root_module.addIncludePath(b.path("Code"));

    const riscv_jit_stencils_disasm_run = b.addRunArtifact(riscv_jit_stencils_disasm);
    riscv_jit_stencils_disasm_run.addFileArg(riscv_jit_stencils.getEmittedBin());
    riscv_jit_stencils_disasm_run.addFileArg(riscv_jit_stencils_src);

    b.getInstallStep().dependOn(&riscv_jit_stencils_disasm_run.step);

    //
    // NOTE: Host
    //
    // Host is broken up in two parts -- the executable and lib. Lib does
    // no care about the actual executable and what it uses for opening window
    // and rendering, it exists as platform-independent utilities that are
    // useful for the host itself but are not supposed to be used by other
    // modules

    // NOTE: Lib

    const host_lib = b.addLibrary(.{
        .name = "host",

        .root_module = b.createModule(.{
            .target = target,
            .optimize = optimize,
        }),

        .linkage = .static,
    });
    host_lib.root_module.addCSourceFiles(.{
        .files = &.{
            "Code/Host/Zip.c",

            "Code/Host/RiscV/RiscV_Instr.c",
            "Code/Host/RiscV/RiscV_JIT.c",
        },
    });

    // NOTE: Exe

    const host_exe = b.addExecutable(.{
        .name = "host",

        .root_module = b.createModule(.{
            .target = target,
            .optimize = optimize,
        }),
    });
    host_exe.root_module.addCSourceFiles(.{
        .files = &.{
            "Code/Host/SDL.c",
            "Code/Host/SDL_Renderer.c",
        },
    });
    host_exe.root_module.linkLibrary(host_lib);
    host_exe.root_module.linkLibrary(sdl.artifact("SDL3"));

    //
    // NOTE: compile_flags.txt
    //

    const compile_flags = b.addWriteFiles().add("compile_flags.txt", b.fmt(
        \\-I
        \\Code
        \\-I
        \\{s}
        \\
        // TODO: getPath4 just makes it kind of ugly here but I guess could be
        // used later.
    , .{sdl.path("include").getPath(b)}));

    const update_compile_flags = b.addUpdateSourceFiles();
    update_compile_flags.addCopyFileToSource(compile_flags, "compile_flags.txt");

    b.getInstallStep().dependOn(&update_compile_flags.step);
}
