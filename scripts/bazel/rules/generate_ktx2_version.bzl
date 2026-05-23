def _generate_ktx2_version_impl(ctx):
    ctx.actions.run(
        inputs = [],
        outputs = [ctx.outputs.output_file],
        executable = ctx.executable._command,
        arguments = [
            ctx.attr.version,
            ctx.attr.default_version,
            ctx.outputs.output_file.path,
        ]
    )

    return DefaultInfo(
        files = depset([ctx.outputs.output_file]),
    )

generate_ktx2_version = rule(
    implementation = _generate_ktx2_version_impl,
    attrs = {
        "version": attr.string(
            mandatory = True,
        ),
        "default_version": attr.string(
            mandatory = True,
        ),
        "output_file": attr.output(
            mandatory = True,
        ),
        "_command": attr.label(
            cfg = "exec",
            executable = True,
            default = Label("@fly//scripts/python:generate_ktx2_version"),
        ),
    },
)