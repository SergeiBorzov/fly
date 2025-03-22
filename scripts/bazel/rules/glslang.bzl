
def _glslang_build_info_impl(ctx):
    ctx.actions.run(
        inputs = [
            ctx.file.input_file,
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

def _glsl_shader_impl(ctx):
    spirv_name = ctx.file.src.basename + ".spv"
    spirv = ctx.actions.declare_file(spirv_name)
    ctx.actions.run(
        inputs = [
            ctx.file.src,
        ],
        outputs = [
            spirv,
        ],
        executable = ctx.executable._command,
        arguments = [
            "-V",
            ctx.file.src.path,
            "-o",
            spirv.path
        ],
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
        "_command": attr.label(
            cfg = "exec",
            executable = True,
            default = Label("@glslang//:glslangValidator"),
        ),
    },
)

