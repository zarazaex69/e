const std = @import("std");
const types = @import("types.zig");
const module_analyzer = @import("module_analyzer.zig");

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
    std.debug.print("Analyzing module: {s}\n\n", .{module_path});

    var analyzer = module_analyzer.ModuleAnalyzer.init(allocator, module_path) catch |err| {
        std.debug.print("Error: Failed to initialize analyzer: {}\n", .{err});
        return;
    };
    defer analyzer.deinit();

    std.debug.print("=== Metadata ===\n", .{});
    const metadata = analyzer.extractMetadata() catch |err| {
        std.debug.print("Error: Failed to extract metadata: {}\n", .{err});
        return;
    };
    defer allocator.free(metadata.name);

    std.debug.print("Name: {s}\n", .{metadata.name});
    std.debug.print("Size: {} bytes\n", .{metadata.size});
    std.debug.print("Type: {s}\n", .{@tagName(metadata.module_type)});
    std.debug.print("Compressed: {}\n\n", .{metadata.compressed});

    std.debug.print("=== Functions ===\n", .{});
    const functions = analyzer.extractFunctions() catch |err| {
        std.debug.print("Error: Failed to extract functions: {}\n", .{err});
        return;
    };
    defer {
        for (functions) |func| {
            allocator.free(func.name);
        }
        allocator.free(functions);
    }

    std.debug.print("Total functions: {}\n", .{functions.len});
    for (functions, 0..) |func, i| {
        if (i < 10) {
            std.debug.print("  0x{x:0>8} {s} (size: {})\n", .{ func.address, func.name, func.size });
        }
    }
    if (functions.len > 10) {
        std.debug.print("  ... and {} more\n", .{functions.len - 10});
    }
    std.debug.print("\n", .{});

    std.debug.print("=== Strings ===\n", .{});
    const strings = analyzer.extractStrings() catch |err| {
        std.debug.print("Error: Failed to extract strings: {}\n", .{err});
        return;
    };
    defer {
        for (strings) |str| {
            allocator.free(str.value);
        }
        allocator.free(strings);
    }

    std.debug.print("Total strings: {}\n", .{strings.len});
    for (strings, 0..) |str, i| {
        if (i < 10) {
            std.debug.print("  0x{x:0>8} \"{s}\"\n", .{ str.address, str.value });
        }
    }
    if (strings.len > 10) {
        std.debug.print("  ... and {} more\n", .{strings.len - 10});
    }
    std.debug.print("\n", .{});

    std.debug.print("=== Entry Point ===\n", .{});
    const entry_point = analyzer.findEntryPoint() catch |err| {
        std.debug.print("Error: Failed to find entry point: {}\n", .{err});
        return;
    };
    std.debug.print("Entry point: 0x{x:0>8}\n", .{entry_point});
}

test "basic test" {
    try std.testing.expect(true);
}
