const std = @import("std");
const buildzon = @import("build.zig.zon");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    var exe = b.addExecutable(.{
        .name = "mus2",
        .target = target,
        .optimize = optimize,
    });

    exe.addCSourceFile(.{
        .file = b.path("src/main.c"),
    });

    if (optimize != .Debug) {
        exe.addWin32ResourceFile(.{
            .file = b.path("assets/app.rc"),
        });
        exe.subsystem = .Windows;
    }

    exe.linkLibC();

    const strap = b.dependency("strap", .{
        .target = target,
        .optimize = optimize,
    });

    const config: []const u8 = "-DSUPPORT_FILEFORMAT_FLAC";
    const raylib = b.dependency("raylib", .{
        .target = target,
        .optimize = optimize,
        .config = config,
        .shared = false,
    });

    exe.linkLibrary(strap.artifact("strap"));
    exe.linkLibrary(raylib.artifact("raylib"));

    const meta = std.fmt.allocPrint(b.allocator,
        \\// auto-generated
        \\#define VERSION "{s}"
        \\#define ARCH "{s}-{s}-{s}"
        \\#define OPTIMIZE "{s}"
        \\#define BUILD_DATE {}
    , .{ buildzon.version, @tagName(target.result.cpu.arch), @tagName(target.result.os.tag), @tagName(target.result.abi), @tagName(optimize), std.time.timestamp() }) catch @panic("OOM");

    const file = b.addWriteFile("meta.h", meta);

    exe.step.dependOn(&file.step);
    exe.addIncludePath(file.getDirectory());

    b.installArtifact(exe);

    const run_cmd = b.addRunArtifact(exe);
    run_cmd.step.dependOn(b.getInstallStep());
    if (b.args) |args| {
        run_cmd.addArgs(args);
    }
    const run_step = b.step("run", "Run the app");
    run_step.dependOn(&run_cmd.step);
}
