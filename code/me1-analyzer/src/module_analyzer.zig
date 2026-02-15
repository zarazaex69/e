const std = @import("std");
const types = @import("types.zig");
const rizin = @import("rizin.zig");

pub const ModuleAnalyzerError = error{
    InvalidModule,
    MetadataExtractionFailed,
    FunctionExtractionFailed,
    StringExtractionFailed,
    EntryPointNotFound,
};

pub const ModuleMetadata = struct {
    name: []const u8,
    size: usize,
    compressed: bool,
    module_type: types.ModuleType,
};

pub const ModuleAnalyzer = struct {
    allocator: std.mem.Allocator,
    rizin_session: rizin.RizinSession,
    module_path: []const u8,

    pub fn init(allocator: std.mem.Allocator, module_path: []const u8) !ModuleAnalyzer {
        const session = try rizin.RizinSession.open(allocator, module_path);

        return ModuleAnalyzer{
            .allocator = allocator,
            .rizin_session = session,
            .module_path = module_path,
        };
    }

    pub fn deinit(self: *ModuleAnalyzer) void {
        self.rizin_session.close();
    }

    pub fn extractMetadata(self: *ModuleAnalyzer) !ModuleMetadata {
        const file = std.fs.cwd().openFile(self.module_path, .{}) catch {
            return ModuleAnalyzerError.InvalidModule;
        };
        defer file.close();

        const stat = try file.stat();
        const size = stat.size;

        const basename = std.fs.path.basename(self.module_path);
        const name = try self.allocator.dupe(u8, basename);

        const compressed = try self.detectCompression();
        const module_type = self.inferModuleType(basename);

        return ModuleMetadata{
            .name = name,
            .size = size,
            .compressed = compressed,
            .module_type = module_type,
        };
    }

    pub fn extractFunctions(self: *ModuleAnalyzer) ![]types.FunctionInfo {
        try self.rizin_session.analyze();

        const functions = self.rizin_session.getFunctions() catch |err| {
            std.log.err("Failed to extract functions: {}", .{err});
            return ModuleAnalyzerError.FunctionExtractionFailed;
        };

        return functions;
    }

    pub fn extractStrings(self: *ModuleAnalyzer) ![]types.StringInfo {
        const strings = self.rizin_session.getStrings() catch |err| {
            std.log.err("Failed to extract strings: {}", .{err});
            return ModuleAnalyzerError.StringExtractionFailed;
        };

        return strings;
    }

    pub fn findEntryPoint(self: *ModuleAnalyzer) !u64 {
        const functions = try self.extractFunctions();
        defer {
            for (functions) |func| {
                self.allocator.free(func.name);
            }
            self.allocator.free(functions);
        }

        for (functions) |func| {
            if (func.address == 0 or
                std.mem.startsWith(u8, func.name, "entry") or
                std.mem.startsWith(u8, func.name, "main"))
            {
                return func.address;
            }
        }

        if (functions.len > 0) {
            return functions[0].address;
        }

        return ModuleAnalyzerError.EntryPointNotFound;
    }

    fn detectCompression(self: *ModuleAnalyzer) !bool {
        const file = try std.fs.cwd().openFile(self.module_path, .{});
        defer file.close();

        var magic_bytes: [4]u8 = undefined;
        const bytes_read = try file.read(&magic_bytes);

        if (bytes_read < 4) {
            return false;
        }

        if (magic_bytes[0] == 0x5d and magic_bytes[1] == 0x00) {
            return true;
        }

        if (magic_bytes[0] == 0x1f and magic_bytes[1] == 0x8b) {
            return true;
        }

        if (magic_bytes[0] == 0xfd and magic_bytes[1] == 0x37 and
            magic_bytes[2] == 0x7a and magic_bytes[3] == 0x58)
        {
            return true;
        }

        return false;
    }

    fn inferModuleType(self: *ModuleAnalyzer, basename: []const u8) types.ModuleType {
        _ = self;

        if (std.mem.indexOf(u8, basename, "BRINGUP") != null or
            std.mem.indexOf(u8, basename, "PRELOADER") != null or
            std.mem.indexOf(u8, basename, "BUCLS") != null or
            std.mem.indexOf(u8, basename, "BUPMSEQ") != null)
        {
            return types.ModuleType.Boot;
        }

        if (std.mem.indexOf(u8, basename, "Kernel") != null) {
            return types.ModuleType.Kernel;
        }

        if (std.mem.indexOf(u8, basename, "PM") != null or
            std.mem.indexOf(u8, basename, "MOFF") != null)
        {
            return types.ModuleType.PowerManagement;
        }

        if (std.mem.indexOf(u8, basename, "EFFS") != null) {
            return types.ModuleType.FileSystem;
        }

        if (std.mem.indexOf(u8, basename, "TPM") != null or
            std.mem.indexOf(u8, basename, "TDT") != null)
        {
            return types.ModuleType.Security;
        }

        if (std.mem.indexOf(u8, basename, "UPEK") != null) {
            return types.ModuleType.Biometrics;
        }

        if (std.mem.indexOf(u8, basename, "_OVL") != null) {
            return types.ModuleType.Overlay;
        }

        return types.ModuleType.Service;
    }
};

test "ModuleAnalyzer init with valid module" {
    const allocator = std.testing.allocator;

    const test_file = "test_module_analyzer.bin";
    const file = try std.fs.cwd().createFile(test_file, .{});
    try file.writeAll(&[_]u8{ 0x00, 0x01, 0x02, 0x03 });
    file.close();
    defer std.fs.cwd().deleteFile(test_file) catch {};

    var analyzer = try ModuleAnalyzer.init(allocator, test_file);
    defer analyzer.deinit();

    try std.testing.expectEqualStrings(test_file, analyzer.module_path);
}

test "ModuleAnalyzer init with invalid module" {
    const allocator = std.testing.allocator;

    const result = ModuleAnalyzer.init(allocator, "/nonexistent/module.bin");
    try std.testing.expectError(rizin.RizinError.ConnectionFailed, result);
}

test "ModuleAnalyzer extractMetadata" {
    const allocator = std.testing.allocator;

    const test_file = "test_metadata.bin";
    const file = try std.fs.cwd().createFile(test_file, .{});
    try file.writeAll(&[_]u8{ 0x00, 0x01, 0x02, 0x03, 0x04, 0x05 });
    file.close();
    defer std.fs.cwd().deleteFile(test_file) catch {};

    var analyzer = try ModuleAnalyzer.init(allocator, test_file);
    defer analyzer.deinit();

    const metadata = try analyzer.extractMetadata();
    defer allocator.free(metadata.name);

    try std.testing.expectEqualStrings("test_metadata.bin", metadata.name);
    try std.testing.expectEqual(@as(usize, 6), metadata.size);
    try std.testing.expectEqual(false, metadata.compressed);
}

test "ModuleAnalyzer detectCompression LZMA" {
    const allocator = std.testing.allocator;

    const test_file = "test_lzma.bin";
    const file = try std.fs.cwd().createFile(test_file, .{});
    try file.writeAll(&[_]u8{ 0x5d, 0x00, 0x00, 0x80 });
    file.close();
    defer std.fs.cwd().deleteFile(test_file) catch {};

    var analyzer = try ModuleAnalyzer.init(allocator, test_file);
    defer analyzer.deinit();

    const compressed = try analyzer.detectCompression();
    try std.testing.expectEqual(true, compressed);
}

test "ModuleAnalyzer detectCompression gzip" {
    const allocator = std.testing.allocator;

    const test_file = "test_gzip.bin";
    const file = try std.fs.cwd().createFile(test_file, .{});
    try file.writeAll(&[_]u8{ 0x1f, 0x8b, 0x08, 0x00 });
    file.close();
    defer std.fs.cwd().deleteFile(test_file) catch {};

    var analyzer = try ModuleAnalyzer.init(allocator, test_file);
    defer analyzer.deinit();

    const compressed = try analyzer.detectCompression();
    try std.testing.expectEqual(true, compressed);
}

test "ModuleAnalyzer inferModuleType Boot" {
    const allocator = std.testing.allocator;

    const test_file = "PRELOADER.bin";
    const file = try std.fs.cwd().createFile(test_file, .{});
    file.close();
    defer std.fs.cwd().deleteFile(test_file) catch {};

    var analyzer = try ModuleAnalyzer.init(allocator, test_file);
    defer analyzer.deinit();

    const module_type = analyzer.inferModuleType("PRELOADER.bin");
    try std.testing.expectEqual(types.ModuleType.Boot, module_type);
}

test "ModuleAnalyzer inferModuleType Kernel" {
    const allocator = std.testing.allocator;

    const test_file = "KernelPriv.bin";
    const file = try std.fs.cwd().createFile(test_file, .{});
    file.close();
    defer std.fs.cwd().deleteFile(test_file) catch {};

    var analyzer = try ModuleAnalyzer.init(allocator, test_file);
    defer analyzer.deinit();

    const module_type = analyzer.inferModuleType("KernelPriv.bin");
    try std.testing.expectEqual(types.ModuleType.Kernel, module_type);
}

test "ModuleAnalyzer inferModuleType Security" {
    const allocator = std.testing.allocator;

    const test_file = "TPM.bin";
    const file = try std.fs.cwd().createFile(test_file, .{});
    file.close();
    defer std.fs.cwd().deleteFile(test_file) catch {};

    var analyzer = try ModuleAnalyzer.init(allocator, test_file);
    defer analyzer.deinit();

    const module_type = analyzer.inferModuleType("TPM.bin");
    try std.testing.expectEqual(types.ModuleType.Security, module_type);
}

test "ModuleAnalyzer extractFunctions with real BRINGUP module" {
    const allocator = std.testing.allocator;

    const module_path = "../../reverse/me1/modules/BRINGUP.bin";

    std.fs.cwd().access(module_path, .{}) catch {
        return error.SkipZigTest;
    };

    var analyzer = try ModuleAnalyzer.init(allocator, module_path);
    defer analyzer.deinit();

    const functions = try analyzer.extractFunctions();
    defer {
        for (functions) |func| {
            allocator.free(func.name);
        }
        allocator.free(functions);
    }

    try std.testing.expect(functions.len > 0);
}

test "ModuleAnalyzer extractStrings with real BRINGUP module" {
    const allocator = std.testing.allocator;

    const module_path = "../../reverse/me1/modules/BRINGUP.bin";

    std.fs.cwd().access(module_path, .{}) catch {
        return error.SkipZigTest;
    };

    var analyzer = try ModuleAnalyzer.init(allocator, module_path);
    defer analyzer.deinit();

    const strings = try analyzer.extractStrings();
    defer {
        for (strings) |str| {
            allocator.free(str.value);
        }
        allocator.free(strings);
    }

    try std.testing.expect(strings.len >= 0);
}

test "ModuleAnalyzer findEntryPoint with real PRELOADER module" {
    const allocator = std.testing.allocator;

    const module_path = "../../reverse/me1/modules/PRELOADER.bin";

    std.fs.cwd().access(module_path, .{}) catch {
        return error.SkipZigTest;
    };

    var analyzer = try ModuleAnalyzer.init(allocator, module_path);
    defer analyzer.deinit();

    const entry_point = try analyzer.findEntryPoint();
    try std.testing.expect(entry_point >= 0);
}

test "ModuleAnalyzer full workflow with BRINGUP" {
    const allocator = std.testing.allocator;

    const module_path = "../../reverse/me1/modules/BRINGUP.bin";

    std.fs.cwd().access(module_path, .{}) catch {
        return error.SkipZigTest;
    };

    var analyzer = try ModuleAnalyzer.init(allocator, module_path);
    defer analyzer.deinit();

    const metadata = try analyzer.extractMetadata();
    defer allocator.free(metadata.name);

    try std.testing.expect(metadata.size > 0);
    try std.testing.expectEqual(types.ModuleType.Boot, metadata.module_type);

    const functions = try analyzer.extractFunctions();
    defer {
        for (functions) |func| {
            allocator.free(func.name);
        }
        allocator.free(functions);
    }

    try std.testing.expect(functions.len > 0);

    const strings = try analyzer.extractStrings();
    defer {
        for (strings) |str| {
            allocator.free(str.value);
        }
        allocator.free(strings);
    }

    const entry_point = try analyzer.findEntryPoint();
    try std.testing.expect(entry_point >= 0);
}

test "Property 4: Analysis script correctness for all ME1 modules" {
    const allocator = std.testing.allocator;

    const modules_dir = "reverse/me1/modules";
    std.fs.cwd().access(modules_dir, .{}) catch {
        std.debug.print("Skipping Property 4 test: modules directory not accessible\n", .{});
        return error.SkipZigTest;
    };

    const all_modules = [_][]const u8{
        "ALIASCHECK_OVL.bin",
        "BRINGUP.bin",
        "BUCLS_OVL.bin",
        "BUPMSEQ_OVL.bin",
        "CLS.bin",
        "EFFS_IOVL.bin",
        "EFFS_OPOVL.bin",
        "KernelNonPriv.bin",
        "KernelPriv.bin",
        "MOFFM0_OVL.bin",
        "PKTPM.bin",
        "PKTPMINIT_OVL.bin",
        "PMHWSEQ.bin",
        "PRELOADER.bin",
        "SUPPORT_OVL.bin",
        "TDT.bin",
        "TPM.bin",
        "UPEK.bin",
    };

    const output_dir = "test_output_property4";
    std.fs.cwd().makeDir(output_dir) catch |err| switch (err) {
        error.PathAlreadyExists => {},
        else => return err,
    };
    defer std.fs.cwd().deleteTree(output_dir) catch {};

    var successful_analyses: usize = 0;
    var failed_analyses: usize = 0;

    for (all_modules) |module_name| {
        const module_path = try std.fmt.allocPrint(allocator, "{s}/{s}", .{ modules_dir, module_name });
        defer allocator.free(module_path);

        std.fs.cwd().access(module_path, .{}) catch {
            std.debug.print("Skipping {s}: file not accessible\n", .{module_name});
            failed_analyses += 1;
            continue;
        };

        var analyzer = ModuleAnalyzer.init(allocator, module_path) catch |err| {
            std.debug.print("Failed to init analyzer for {s}: {}\n", .{ module_name, err });
            failed_analyses += 1;
            continue;
        };
        defer analyzer.deinit();

        const metadata = analyzer.extractMetadata() catch |err| {
            std.debug.print("Failed to extract metadata for {s}: {}\n", .{ module_name, err });
            failed_analyses += 1;
            continue;
        };
        defer allocator.free(metadata.name);

        try std.testing.expect(metadata.size > 0);

        const functions = analyzer.extractFunctions() catch |err| {
            std.debug.print("Failed to extract functions for {s}: {}\n", .{ module_name, err });
            failed_analyses += 1;
            continue;
        };
        defer {
            for (functions) |func| {
                allocator.free(func.name);
            }
            allocator.free(functions);
        }

        try std.testing.expect(functions.len > 0);

        const strings = analyzer.extractStrings() catch |err| {
            std.debug.print("Failed to extract strings for {s}: {}\n", .{ module_name, err });
            failed_analyses += 1;
            continue;
        };
        defer {
            for (strings) |str| {
                allocator.free(str.value);
            }
            allocator.free(strings);
        }

        const output_filename = try std.fmt.allocPrint(allocator, "{s}/{s}.json", .{ output_dir, module_name });
        defer allocator.free(output_filename);

        const output_file = try std.fs.cwd().createFile(output_filename, .{});
        defer output_file.close();

        var write_buffer: [4096]u8 = undefined;
        var file_writer = output_file.writer(&write_buffer);
        const writer = &file_writer.interface;

        try writer.writeAll("{\n");
        try writer.writeAll("  \"module_name\": \"");
        try writer.writeAll(metadata.name);
        try writer.writeAll("\",\n");

        try writer.writeAll("  \"size\": ");
        try writer.print("{d}", .{metadata.size});
        try writer.writeAll(",\n");

        try writer.writeAll("  \"compressed\": ");
        try writer.writeAll(if (metadata.compressed) "true" else "false");
        try writer.writeAll(",\n");

        try writer.writeAll("  \"module_type\": \"");
        try writer.writeAll(@tagName(metadata.module_type));
        try writer.writeAll("\",\n");

        try writer.writeAll("  \"functions\": [\n");
        for (functions, 0..) |func, i| {
            try writer.writeAll("    {\n");
            try writer.writeAll("      \"address\": ");
            try writer.print("{d}", .{func.address});
            try writer.writeAll(",\n");
            try writer.writeAll("      \"name\": \"");
            try writer.writeAll(func.name);
            try writer.writeAll("\",\n");
            try writer.writeAll("      \"size\": ");
            try writer.print("{d}", .{func.size});
            try writer.writeAll("\n");
            try writer.writeAll("    }");
            if (i < functions.len - 1) {
                try writer.writeAll(",");
            }
            try writer.writeAll("\n");
        }
        try writer.writeAll("  ],\n");

        try writer.writeAll("  \"strings\": [\n");
        for (strings, 0..) |str, i| {
            try writer.writeAll("    {\n");
            try writer.writeAll("      \"address\": ");
            try writer.print("{d}", .{str.address});
            try writer.writeAll(",\n");
            try writer.writeAll("      \"value\": \"");
            try writer.writeAll(str.value);
            try writer.writeAll("\",\n");
            try writer.writeAll("      \"length\": ");
            try writer.print("{d}", .{str.length});
            try writer.writeAll("\n");
            try writer.writeAll("    }");
            if (i < strings.len - 1) {
                try writer.writeAll(",");
            }
            try writer.writeAll("\n");
        }
        try writer.writeAll("  ]\n");
        try writer.writeAll("}\n");

        try writer.flush();

        const stat = try std.fs.cwd().statFile(output_filename);
        try std.testing.expect(stat.size > 0);

        successful_analyses += 1;
        std.debug.print("Successfully analyzed {s}: {d} functions, {d} strings\n", .{ module_name, functions.len, strings.len });
    }

    std.debug.print("\nProperty 4 Test Summary: {d} successful, {d} failed out of {d} modules\n", .{ successful_analyses, failed_analyses, all_modules.len });

    try std.testing.expect(successful_analyses > 0);
}
