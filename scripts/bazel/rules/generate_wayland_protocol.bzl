def _generate_wayland_protocol_impl(ctx):
    ctx.actions.run(
        inputs = [ctx.file.protocol_file],
        outputs = [ctx.outputs.output_file],
        executable = ctx.executable._command,
        arguments = [
            ctx.attr.option,
            ctx.file.protocol_file.path,
            ctx.outputs.output_file.path,
        ]
    )

    return DefaultInfo(
        files = depset([ctx.outputs.output_file]),
    )

generate_wayland_protocol = rule(
    implementation = _generate_wayland_protocol_impl,
    attrs = {
        "protocol_file": attr.label(
            mandatory = True,
            allow_single_file = True,
        ),
        "output_file": attr.output(
            mandatory = True,
        ),
        "option": attr.string(
            mandatory = True,
            values = [
                "client-header",
                "server-header",
                "enum-header",
                "private-code",
                "public-code",
            ],
        ),
        "_command": attr.label(
            cfg = "exec",
            executable = True,
            default = Label("@fly//scripts/python:generate_wayland_protocol"),
        ),
    },
)
