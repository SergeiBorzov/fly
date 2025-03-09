
def _glslang_build_info_impl(ctx):
    args = ctx.actions.args()
    
    args.add(ctx.file.input_file)
    args.add(ctx.outputs.output_file)

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
    }
)
