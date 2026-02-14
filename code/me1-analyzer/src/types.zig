const std = @import("std");

pub const ModuleType = enum {
    Boot,
    Kernel,
    PowerManagement,
    FileSystem,
    Security,
    Biometrics,
    Service,
    Overlay,
};

pub const ModuleInfo = struct {
    name: []const u8,
    file_path: []const u8,
    module_type: ModuleType,
    size: usize,
    compressed: bool,
    entry_point: u64,
    load_address: u64,
    functions: []FunctionInfo,
    strings: []StringInfo,
    exports: []ExportInfo,
    imports: []ImportInfo,
    dependencies: [][]const u8,
};

pub const FunctionInfo = struct {
    address: u64,
    name: []const u8,
    size: usize,
    signature: FunctionSignature,
    basic_blocks: []BasicBlock,
    callees: []u64,
    callers: []u64,
    xrefs_to: []Xref,
    xrefs_from: []Xref,
    complexity: u32,
};

pub const FunctionSignature = struct {
    return_type: []const u8,
    parameters: []Parameter,
};

pub const Parameter = struct {
    name: []const u8,
    param_type: []const u8,
};

pub const BasicBlock = struct {
    address: u64,
    size: usize,
    instructions: []Instruction,
};

pub const Instruction = struct {
    address: u64,
    mnemonic: []const u8,
    operands: []const u8,
    bytes: []u8,
};

pub const StringInfo = struct {
    address: u64,
    value: []const u8,
    length: usize,
};

pub const ExportInfo = struct {
    name: []const u8,
    address: u64,
    export_type: []const u8,
};

pub const ImportInfo = struct {
    name: []const u8,
    address: u64,
    target_module: ?[]const u8,
    target_address: ?u64,
    status: ImportStatus,
};

pub const ImportStatus = enum {
    Resolved,
    Unresolved,
};

pub const Xref = struct {
    from_address: u64,
    to_address: u64,
    xref_type: XrefType,
};

pub const XrefType = enum {
    Call,
    Jump,
    Data,
};

pub const BootStage = struct {
    order: u32,
    module_name: []const u8,
    entry_address: u64,
    description: []const u8,
    calls: []FunctionCall,
    conditions: []BranchCondition,
};

pub const FunctionCall = struct {
    from_address: u64,
    to_address: u64,
    function_name: []const u8,
    parameters: []Parameter,
};

pub const BranchCondition = struct {
    address: u64,
    condition: []const u8,
    taken_target: u64,
    not_taken_target: u64,
};

pub const Severity = enum {
    Critical,
    High,
    Medium,
    Low,
    Info,
};

pub const Vulnerability = struct {
    id: []const u8,
    title: []const u8,
    severity: Severity,
    description: []const u8,
    affected_modules: [][]const u8,
    proof_of_concept: []const u8,
    impact: []const u8,
    mitigation: []const u8,
};

pub const DependencyEdge = struct {
    from_module: []const u8,
    to_module: []const u8,
    edge_type: []const u8,
};

pub const ModuleNode = struct {
    name: []const u8,
    module_type: ModuleType,
};
