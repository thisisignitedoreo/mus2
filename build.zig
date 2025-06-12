const std = @import("std");

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

    const raylib = b.dependency("raylib", .{
        .target = target,
        .optimize = optimize,
        .linux_display_backend = .X11,
    });

    exe.linkLibrary(strap.artifact("strap"));
    exe.linkLibrary(raylib.artifact("raylib"));

    b.installArtifact(exe);

    const run_cmd = b.addRunArtifact(exe);
    run_cmd.step.dependOn(b.getInstallStep());
    if (b.args) |args| {
        run_cmd.addArgs(args);
    }
    const run_step = b.step("run", "Run the app");
    run_step.dependOn(&run_cmd.step);
}
