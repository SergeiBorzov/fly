
def _glslang_build_info_impl(ctx):
    ctx.actions.run(
        inputs = [
            ctx.file.input_file,
            ctx.file.changes_md_file,
        ],
        outputs = [
            ctx.outputs.output_file,
        ],
        executable = ctx.executable._command,
        arguments = [
            ctx.attr.directory,
            "-i",
            ctx.file.input_file.path,
            "-o",
            ctx.outputs.output_file.path,
        ],
    )

    return [DefaultInfo(files = depset([ctx.outputs.output_file]))]

glslang_build_info = rule(
    implementation = _glslang_build_info_impl,
    attrs = {
        "input_file": attr.label(
            mandatory = True,
            allow_single_file = [".tmpl"],
        ),
        "changes_md_file": attr.label(
            mandatory = True,
            allow_single_file = [".md"],
        ),
        "directory": attr.string(
            mandatory = True,
        ),
        "output_file": attr.output(
            mandatory = True,
        ),
        "_command": attr.label(
            cfg = "exec",
            executable = True,
            default = Label("@glslang//:build_info"),
        ),
    },
)

def _glslang_gen_extension_headers_impl(ctx):
    ctx.actions.run(
        inputs = [
        ],
        outputs = [
            ctx.outputs.output_file,
        ],
        executable = ctx.executable._command,
        arguments = [
            "-i",
            ctx.attr.input_directory,
            "-o",
            ctx.outputs.output_file.path,
        ],
    )

    return [DefaultInfo(files = depset([ctx.outputs.output_file]))]


glslang_gen_extension_headers = rule(
    implementation = _glslang_gen_extension_headers_impl,
    attrs = {
        "input_directory": attr.string(
            mandatory = True,
        ),
        "output_file": attr.output(
            mandatory = True,
        ),
        "_command": attr.label(
            cfg = "exec",
            executable = True,
            default = Label("@glslang//:gen_extension_headers"),
        ),
    },
)

GlslInfo = provider(
    fields = [
        "includes",
        "headers",
    ],
)

def _glsl_library_impl(ctx):
    includes = {}
    for hdr in ctx.files.hdrs:
        includes[hdr.dirname] = True

    runfiles = ctx.runfiles(files = ctx.files.hdrs)
    default_files = depset(direct = ctx.files.hdrs)

    return [
        GlslInfo(
            headers = depset(direct = ctx.files.hdrs),
            includes = depset(direct = includes.keys())
        ),
        DefaultInfo(files = default_files, runfiles = runfiles),
    ]
    
glsl_library = rule(
    implementation = _glsl_library_impl,
    attrs = {
        "hdrs": attr.label_list(
            mandatory = True,
            allow_files = True,
        ),
    },
    provides = [DefaultInfo, GlslInfo],
)

def _glsl_shader_impl(ctx):
    spirv_name = ctx.file.src.basename + ".spv"
    spirv = ctx.actions.declare_file(spirv_name)

    include_args = []
    include_inputs = []
    for dep in ctx.attr.deps:
        for inc in dep[GlslInfo].includes.to_list():
            include_args.append("-I{}".format(inc))

        for hdr in dep[GlslInfo].headers.to_list():
            include_inputs.append(hdr)
            
    ctx.actions.run(
        inputs = [
            ctx.file.src,
        ] + include_inputs,
        outputs = [
            spirv,
        ],
        executable = ctx.executable._command,
        arguments = [
            "-V",
            ctx.file.src.path,
            "-o",
            spirv.path
        ] + include_args,
    )

    return [DefaultInfo(files = depset([spirv]))]

glsl_shader = rule(
    implementation = _glsl_shader_impl,
    attrs = {
        "src": attr.label(
            mandatory = True,
            allow_single_file = [
                ".vert",
                ".tesc",
                ".tese",
                ".geom",
                ".frag",
                ".comp",
                ".mesh",
                ".task",
                ".rgen",
                ".rint",
                ".rahit",
                ".rchit",
                ".rmiss",
                ".rcall",
            ],
        ),
        "deps": attr.label_list(
            mandatory = False,
            providers = [GlslInfo],
        ),
        "_command": attr.label(
            cfg = "exec",
            executable = True,
            default = Label("@glslang//:glslangValidator"),
        ),
    },
)

