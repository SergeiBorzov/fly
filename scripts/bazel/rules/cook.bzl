def _cook_images_impl(ctx):
    outs = []
    for f in ctx.files.outputs:
        out = ctx.actions.declare_file(f.basename)
        outs.append(out)

    ctx.actions.run(
        inputs = ctx.files.inputs,
        outputs = outs,
        executable = ctx.executable._command,
        arguments = [
            "-i"
        ] +
        [f.path for f in ctx.files.inputs] + [
            "-o"
        ]
        + [f.path for f in outs],
    )

    return [DefaultInfo(files = depset(outs))]

cook_images = rule(
    implementation = _cook_images_impl,
    attrs = {
        "inputs": attr.label_list(
            mandatory = True,
            allow_empty = False,
            allow_files = True,
        ),
        "outputs": attr.label_list(
            mandatory = True,
            allow_empty = False,
            allow_files = True,
        ),
        "_command": attr.label(
            cfg = "exec",
            executable = True,
            default = Label("//src/assets:cooker"),
        ),
    },
)
