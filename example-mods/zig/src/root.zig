const std = @import("std");
const sdk = @import("sdk");

var ziggy: i32 = -1;

pub fn init() !void {
    const metadata: sdk.Metadata = .{ .name = "zig mod", .summary = "a nice zig mod" };
    try metadata.set();

    ziggy = sdk.registerAction("ziggy");
    sdk.registerDefaultKey("ziggy", "z");
}

pub fn update(state: *sdk.State, buffer: *sdk.RenderCommandBuffer) !void {
    try buffer.draw(.{ .draw_rectangle = .{
        .color = .green,
        .rectangle = .{
            .position = .{ .x = 10, .y = 2 },
            .size = .{ .x = 10, .y = 10 },
        },
    } });

    if (state.input.custom[@intCast(ziggy)].pressed) {
        std.log.info("{any}", .{state.player.position});
        state.player.position = .{ .x = 0.0, .y = 0.0 };
    }
}
