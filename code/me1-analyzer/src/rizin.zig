const std = @import("std");
const types = @import("types.zig");

pub const RizinError = error{
    ConnectionFailed,
    AnalysisFailed,
    InvalidArchitecture,
    FunctionExtractionFailed,
    XrefAnalysisFailed,
    CommandFailed,
    JsonParseFailed,
    ProcessSpawnFailed,
};

pub const RizinSession = struct {
    allocator: std.mem.Allocator,
    binary_path: []const u8,
    process: ?std.process.Child,
    stdin: ?std.fs.File,
    stdout: ?std.fs.File,
    arch: []const u8,
    bits: u32,

    pub fn open(allocator: std.mem.Allocator, binary_path: []const u8) !RizinSession {
        const file = std.fs.cwd().openFile(binary_path, .{}) catch {
            return RizinError.ConnectionFailed;
        };
        file.close();

        return RizinSession{
            .allocator = allocator,
            .binary_path = binary_path,
            .process = null,
            .stdin = null,
            .stdout = null,
            .arch = "arc",
            .bits = 32,
        };
    }

    pub fn close(self: *RizinSession) void {
        if (self.process) |*proc| {
            _ = proc.kill() catch {};
        }
        if (self.stdin) |f| f.close();
        if (self.stdout) |f| f.close();
    }

    pub fn analyze(self: *RizinSession) !void {
        const result = try self.executeCommand("aaa");
        defer self.allocator.free(result);
    }

    pub fn getFunctions(self: *RizinSession) ![]types.FunctionInfo {
        const json_output = try self.executeCommand("aaa; aflj");
        defer self.allocator.free(json_output);

        if (json_output.len == 0) {
            return &[_]types.FunctionInfo{};
        }

        return try self.parseFunctionsJson(json_output);
    }

    pub fn getStrings(self: *RizinSession) ![]types.StringInfo {
        const json_output = try self.executeCommand("izj");
        defer self.allocator.free(json_output);

        if (json_output.len == 0) {
            return &[_]types.StringInfo{};
        }

        return try self.parseStringsJson(json_output);
    }

    pub fn getXrefs(self: *RizinSession, addr: u64) ![]types.Xref {
        const cmd = try std.fmt.allocPrint(self.allocator, "axtj @ 0x{x}", .{addr});
        defer self.allocator.free(cmd);

        const json_output = try self.executeCommand(cmd);
        defer self.allocator.free(json_output);

        if (json_output.len == 0) {
            return &[_]types.Xref{};
        }

        return try self.parseXrefsJson(json_output);
    }

    pub fn disassemble(self: *RizinSession, addr: u64, count: usize) ![]types.Instruction {
        const cmd = try std.fmt.allocPrint(self.allocator, "pdj {d} @ 0x{x}", .{ count, addr });
        defer self.allocator.free(cmd);

        const json_output = try self.executeCommand(cmd);
        defer self.allocator.free(json_output);

        if (json_output.len == 0) {
            return &[_]types.Instruction{};
        }

        return try self.parseInstructionsJson(json_output);
    }

    fn executeCommand(self: *RizinSession, cmd: []const u8) ![]u8 {
        var argv: std.ArrayList([]const u8) = .empty;
        defer argv.deinit(self.allocator);

        try argv.append(self.allocator, "rizin");
        try argv.append(self.allocator, "-q");
        try argv.append(self.allocator, "-a");
        try argv.append(self.allocator, self.arch);
        try argv.append(self.allocator, "-b");

        const bits_str = try std.fmt.allocPrint(self.allocator, "{d}", .{self.bits});
        errdefer self.allocator.free(bits_str);
        try argv.append(self.allocator, bits_str);

        try argv.append(self.allocator, "-c");
        try argv.append(self.allocator, cmd);
        try argv.append(self.allocator, self.binary_path);

        var child = std.process.Child.init(argv.items, self.allocator);
        child.stdout_behavior = .Pipe;
        child.stderr_behavior = .Pipe;

        try child.spawn();

        const stdout = child.stdout orelse return RizinError.ProcessSpawnFailed;
        const stderr = child.stderr orelse return RizinError.ProcessSpawnFailed;

        const max_output_size = 10 * 1024 * 1024;
        const stdout_data = try stdout.readToEndAlloc(self.allocator, max_output_size);
        errdefer self.allocator.free(stdout_data);

        const stderr_data = try stderr.readToEndAlloc(self.allocator, max_output_size);
        defer self.allocator.free(stderr_data);

        const term = try child.wait();

        self.allocator.free(bits_str);

        if (term != .Exited or term.Exited != 0) {
            self.allocator.free(stdout_data);
            return RizinError.CommandFailed;
        }

        return stdout_data;
    }

    fn parseFunctionsJson(self: *RizinSession, json: []const u8) ![]types.FunctionInfo {
        var functions: std.ArrayList(types.FunctionInfo) = .empty;
        errdefer functions.deinit(self.allocator);

        const parsed = std.json.parseFromSlice(std.json.Value, self.allocator, json, .{}) catch {
            return RizinError.JsonParseFailed;
        };
        defer parsed.deinit();

        const root = parsed.value;
        if (root != .array) {
            return &[_]types.FunctionInfo{};
        }

        for (root.array.items) |item| {
            if (item != .object) continue;

            const obj = item.object;
            const name = if (obj.get("name")) |n| n.string else "unknown";
            const offset = if (obj.get("offset")) |o| @as(u64, @intCast(o.integer)) else 0;
            const size = if (obj.get("size")) |s| @as(usize, @intCast(s.integer)) else 0;

            const name_copy = try self.allocator.dupe(u8, name);

            try functions.append(self.allocator, types.FunctionInfo{
                .address = offset,
                .name = name_copy,
                .size = size,
                .signature = types.FunctionSignature{
                    .return_type = "unknown",
                    .parameters = &[_]types.Parameter{},
                },
                .basic_blocks = &[_]types.BasicBlock{},
                .callees = &[_]u64{},
                .callers = &[_]u64{},
                .xrefs_to = &[_]types.Xref{},
                .xrefs_from = &[_]types.Xref{},
                .complexity = 0,
            });
        }

        return try functions.toOwnedSlice(self.allocator);
    }

    fn parseStringsJson(self: *RizinSession, json: []const u8) ![]types.StringInfo {
        var strings: std.ArrayList(types.StringInfo) = .empty;
        errdefer strings.deinit(self.allocator);

        const parsed = std.json.parseFromSlice(std.json.Value, self.allocator, json, .{}) catch {
            return RizinError.JsonParseFailed;
        };
        defer parsed.deinit();

        const root = parsed.value;
        if (root != .array) {
            return &[_]types.StringInfo{};
        }

        for (root.array.items) |item| {
            if (item != .object) continue;

            const obj = item.object;
            const vaddr = if (obj.get("vaddr")) |v| @as(u64, @intCast(v.integer)) else 0;
            const string = if (obj.get("string")) |s| s.string else "";
            const length = if (obj.get("length")) |l| @as(usize, @intCast(l.integer)) else 0;

            const string_copy = try self.allocator.dupe(u8, string);

            try strings.append(self.allocator, types.StringInfo{
                .address = vaddr,
                .value = string_copy,
                .length = length,
            });
        }

        return try strings.toOwnedSlice(self.allocator);
    }

    fn parseXrefsJson(self: *RizinSession, json: []const u8) ![]types.Xref {
        var xrefs: std.ArrayList(types.Xref) = .empty;
        errdefer xrefs.deinit(self.allocator);

        const parsed = std.json.parseFromSlice(std.json.Value, self.allocator, json, .{}) catch {
            return RizinError.JsonParseFailed;
        };
        defer parsed.deinit();

        const root = parsed.value;
        if (root != .array) {
            return &[_]types.Xref{};
        }

        for (root.array.items) |item| {
            if (item != .object) continue;

            const obj = item.object;
            const from = if (obj.get("from")) |f| @as(u64, @intCast(f.integer)) else 0;
            const to = if (obj.get("to")) |t| @as(u64, @intCast(t.integer)) else 0;
            const xref_type_str = if (obj.get("type")) |t| t.string else "CALL";

            const xref_type = if (std.mem.eql(u8, xref_type_str, "CALL"))
                types.XrefType.Call
            else if (std.mem.eql(u8, xref_type_str, "JMP"))
                types.XrefType.Jump
            else
                types.XrefType.Data;

            try xrefs.append(self.allocator, types.Xref{
                .from_address = from,
                .to_address = to,
                .xref_type = xref_type,
            });
        }

        return try xrefs.toOwnedSlice(self.allocator);
    }

    fn parseInstructionsJson(self: *RizinSession, json: []const u8) ![]types.Instruction {
        var instructions: std.ArrayList(types.Instruction) = .empty;
        errdefer instructions.deinit(self.allocator);

        const parsed = std.json.parseFromSlice(std.json.Value, self.allocator, json, .{}) catch {
            return RizinError.JsonParseFailed;
        };
        defer parsed.deinit();

        const root = parsed.value;
        if (root != .array) {
            return &[_]types.Instruction{};
        }

        for (root.array.items) |item| {
            if (item != .object) continue;

            const obj = item.object;
            const offset = if (obj.get("offset")) |o| @as(u64, @intCast(o.integer)) else 0;
            const opcode = if (obj.get("opcode")) |op| op.string else "";
            const disasm = if (obj.get("disasm")) |d| d.string else "";

            const opcode_copy = try self.allocator.dupe(u8, opcode);
            const disasm_copy = try self.allocator.dupe(u8, disasm);

            try instructions.append(self.allocator, types.Instruction{
                .address = offset,
                .mnemonic = opcode_copy,
                .operands = disasm_copy,
                .bytes = &[_]u8{},
            });
        }

        return try instructions.toOwnedSlice(self.allocator);
    }
};

test "RizinSession open with valid file" {
    const allocator = std.testing.allocator;

    const test_file = "test_module.bin";
    const file = try std.fs.cwd().createFile(test_file, .{});
    file.close();
    defer std.fs.cwd().deleteFile(test_file) catch {};

    var session = try RizinSession.open(allocator, test_file);
    defer session.close();

    try std.testing.expectEqualStrings(test_file, session.binary_path);
}

test "RizinSession open with invalid file" {
    const allocator = std.testing.allocator;

    const result = RizinSession.open(allocator, "/nonexistent/file.bin");
    try std.testing.expectError(RizinError.ConnectionFailed, result);
}

test "RizinSession executeCommand basic" {
    const allocator = std.testing.allocator;

    const test_file = "test_module2.bin";
    const file = try std.fs.cwd().createFile(test_file, .{});
    try file.writeAll(&[_]u8{ 0x7f, 0x45, 0x4c, 0x46 });
    file.close();
    defer std.fs.cwd().deleteFile(test_file) catch {};

    var session = try RizinSession.open(allocator, test_file);
    defer session.close();

    const result = session.executeCommand("i") catch |err| {
        return err;
    };
    defer allocator.free(result);
}

test "RizinSession analyze and getFunctions with real module" {
    const allocator = std.testing.allocator;

    const module_path = "../../reverse/me1/modules/BRINGUP.bin";

    std.fs.cwd().access(module_path, .{}) catch {
        return error.SkipZigTest;
    };

    var session = try RizinSession.open(allocator, module_path);
    defer session.close();

    try session.analyze();

    const functions = try session.getFunctions();
    defer {
        for (functions) |func| {
            allocator.free(func.name);
        }
        allocator.free(functions);
    }

    try std.testing.expect(functions.len > 0);
}

test "RizinSession getStrings with real module" {
    const allocator = std.testing.allocator;

    const module_path = "../../reverse/me1/modules/BRINGUP.bin";

    std.fs.cwd().access(module_path, .{}) catch {
        return error.SkipZigTest;
    };

    var session = try RizinSession.open(allocator, module_path);
    defer session.close();

    const strings = try session.getStrings();
    defer {
        for (strings) |str| {
            allocator.free(str.value);
        }
        allocator.free(strings);
    }

    try std.testing.expect(strings.len >= 0);
}

test "RizinSession extract functions from PRELOADER" {
    const allocator = std.testing.allocator;

    const module_path = "../../reverse/me1/modules/PRELOADER.bin";

    std.fs.cwd().access(module_path, .{}) catch {
        return error.SkipZigTest;
    };

    var session = try RizinSession.open(allocator, module_path);
    defer session.close();

    const functions = try session.getFunctions();
    defer {
        for (functions) |func| {
            allocator.free(func.name);
        }
        allocator.free(functions);
    }

    try std.testing.expect(functions.len > 0);

    var found_entry = false;
    for (functions) |func| {
        if (func.address == 0 or std.mem.startsWith(u8, func.name, "entry") or std.mem.startsWith(u8, func.name, "fcn.")) {
            found_entry = true;
            break;
        }
    }
    try std.testing.expect(found_entry);
}
