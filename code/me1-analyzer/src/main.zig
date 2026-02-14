const std = @import("std");
const types = @import("types.zig");

pub fn main() !void {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    std.debug.print("ME1 Analyzer v0.1.0\n", .{});

    const args = try std.process.argsAlloc(allocator);
    defer std.process.argsFree(allocator, args);

    if (args.len < 2) {
        std.debug.print("Usage: me1-analyzer <module.bin>\n", .{});
        return;
    }

    const module_path = args[1];
    std.debug.print("Analyzing module: {s}\n", .{module_path});
}

test "basic test" {
    try std.testing.expect(true);
}
